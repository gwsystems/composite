use xmas_elf::{ElfFile, header, program};
use xmas_elf::sections;
use xmas_elf::sections::{SectionData};
use xmas_elf::symbol_table::{Entry, Entry32, DynEntry32, DynEntry64, Type, Binding};
use xmas_elf::sections::{SHF_ALLOC, SHF_COMPRESSED, SHF_WRITE, SHF_EXECINSTR, SHF_GROUP, SHF_INFO_LINK, SHF_LINK_ORDER, SHF_MASKOS, SHF_MASKPROC, SHF_MERGE, SHF_OS_NONCONFORMING, SHF_TLS, SHF_STRINGS};

use symbols::Symb;
use std::collections::HashMap;
use passes::{SystemState, BuildState, ObjectsPass, ConstructorPass, Transition, TransitionIter, Component, ComponentId, VAddr, ClientSymb, CompSymbs, ComponentName, component};
use syshelpers::{dump_file, exec_pipeline};
use itertools::Itertools;
use std::fs::File;

struct ClientSymbol {
    name: String,
    func_addr: u64,
    ucap_addr: u64
}

impl ClientSymbol {
    pub fn name(&self) -> &String {
        &self.name
    }

    pub fn func_addr(&self) -> u64 {
        self.func_addr
    }

    pub fn ucap_addr(&self) -> u64 {
        self.ucap_addr
    }
}

struct ServerSymb {
    name: String,
    addr: u64
}

impl ServerSymb {
    pub fn name(&self) -> &String {
        &self.name
    }

    pub fn addr(&self) -> u64 {
        self.addr
    }
}

struct CompObject {
    name: String,
    dep_symbs: Vec<ClientSymbol>,
    exp_symbs: Vec<ServerSymb>,
    compinfo_symb: u64,
    entryfn_symb: u64,
    defclifn_symb: u64,

}

impl CompObject {
    pub fn parse(name: &String, obj: &Vec<u8>) -> Result<CompObject, String> {
        let elf_file = ElfFile::new(obj).unwrap();
        let symbs    = symbs_retrieve(&elf_file)?;

        let compinfo = compinfo_addr(&elf_file, symbs)?.addr();
        let entryfn  = entry_addr(&elf_file, symbs)?.addr();
        let defclifn = defcli_stub_addr(&elf_file, symbs)?.addr();

        let exps     = compute_exports(&elf_file, symbs)?;
        let deps     = compute_dependencies(&elf_file, symbs)?;

        Ok(CompObject {
            name: name.clone(),
            dep_symbs: deps,
            exp_symbs: exps,
            compinfo_symb: compinfo,
            entryfn_symb: entryfn,
            defclifn_symb: defclifn
        })
    }

    pub fn name(&self) -> &String {
        &self.name
    }

    pub fn exported(&self) -> &Vec<ServerSymb> {
        &self.exp_symbs
    }

    pub fn dependencies(&self) -> &Vec<ClientSymbol> {
        &self.dep_symbs
    }

    pub fn compinfo_addr(&self) -> u64 {
        self.compinfo_symb
    }

    pub fn entryfn_addr(&self) -> u64 {
        self.entryfn_symb
    }
}

fn symb_address<'a>(e: &ElfFile<'a>, symb: &'a Entry32) -> u64 {
    symb.value()
}

fn global_variables<'a>(e: &ElfFile<'a>, symbs: &'a [Entry32]) -> Vec<Symb<'a>> {
    symbs.iter().filter_map(
        |symb| match (symb.get_name(&e), symb.get_binding(), symb.get_type()) {
            (Ok(name), Ok(Binding::Global), Ok(Type::NoType)) |
            (Ok(name), Ok(Binding::Global), Ok(Type::Object)) => Some(Symb::new(name, symb_address(e, symb))),
            _ => None
        }
    ).collect()
}

fn global_functions<'a>(e: &ElfFile<'a>, symbs: &'a [Entry32]) -> Vec<Symb<'a>> {
    symbs.iter().filter_map(
        |symb| match (symb.get_name(&e), symb.get_binding(), symb.get_type()) {
            (Ok(name), Ok(Binding::Global), Ok(Type::Func)) => Some(Symb::new(name, symb_address(e, symb))),
            _ => None
        }
    ).collect()
}

fn symbol_prefix_filter<'a>(e: &ElfFile<'a>, symbs: &'a [Entry32], prefix: &str, symb_filter: fn(&ElfFile<'a>, &'a [Entry32]) -> Vec<Symb<'a>>) -> Vec<Symb<'a>> {
    symb_filter(e, symbs).iter().filter_map(
        |ref symb| if symb.name().starts_with(prefix) { Some(Symb::new(&symb.name()[prefix.len()..], symb.addr())) } else { None }
    ).collect()
}

fn client_caps<'a>(e: &ElfFile<'a>, symbs: &'a [Entry32]) -> Vec<Symb<'a>> {
    symbol_prefix_filter(e, symbs, "__cosrt_ucap_", global_variables)
}

fn client_stubs<'a>(e: &ElfFile<'a>, symbs: &'a [Entry32]) -> Vec<Symb<'a>> {
    symbol_prefix_filter(e, symbs, "__cosrt_c_", global_functions)
}

fn server_stubs<'a>(e: &ElfFile<'a>, symbs: &'a [Entry32]) -> Vec<Symb<'a>> {
    symbol_prefix_filter(e, symbs, "__cosrt_s_", global_functions)
}

fn unique_symbol<T>(symbs: &mut Vec<T>) -> Option<T> {
    if symbs.len() != 1 {
        return None;
    }
    if let Some(s) = symbs.pop() {
        Some(s)
    } else {
        None
    }
}

fn compinfo_addr<'a>(e: &ElfFile<'a>, symbs: &'a [Entry32]) -> Result<Symb<'a>, String> {
    unique_symbol(&mut symbol_prefix_filter(e, symbs, "__cosrt_comp_info", global_variables))
        .ok_or(String::from("Could not find the __cosrt_upcall_entry function."))
}

fn entry_addr<'a>(e: &ElfFile<'a>, symbs: &'a [Entry32]) -> Result<Symb<'a>, String> {
    unique_symbol(&mut symbol_prefix_filter(e, symbs, "__cosrt_upcall_entry", global_functions))
        .ok_or(String::from("Could not find the __cosrt_upcall_entry function."))
}

fn defcli_stub_addr<'a>(e: &ElfFile<'a>, symbs: &'a [Entry32]) -> Result<Symb<'a>, String> {
    unique_symbol(&mut symbol_prefix_filter(e, symbs, "__cosrt_c_cosrtdefault", global_functions))
        .ok_or(String::from("Could not find the __cosrt_c_cosrtdefault function."))
}

fn symbs_retrieve<'a>(e: &ElfFile<'a>) -> Result<&'a [Entry32], String> {
    match e.find_section_by_name(".symtab").unwrap().get_data(&e) {
        //Ok(SectionData::DynSymbolTable32(sts)) => section_symbols_print(e, sts),
        Ok(SectionData::SymbolTable32(ref sts)) => Ok(sts),
        _ => Err(String::from("Could not find the symbol table."))
    }
}

fn compute_dependencies<'a>(e: &ElfFile<'a>, symbs: &'a [Entry32]) -> Result<Vec<ClientSymbol>, String> {
    let defstub    = defcli_stub_addr(e, symbs)?;
    let ucap_symbs = client_caps(e, symbs);
    let dep_symbs  = client_stubs(e, symbs);

    Ok(ucap_symbs.iter().map(|s| {
        let stub = dep_symbs.iter().fold(None, |found, next| {
            if next.name() == s.name() {
                Some(next.addr())
            } else {
                found
            }}).unwrap_or(defstub.addr());
        ClientSymbol {
            name: String::from(s.name()),
            func_addr: stub,
            ucap_addr: s.addr()
        }
    }).collect())
}

fn compute_exports<'a>(e: &ElfFile<'a>, symbs: &'a [Entry32]) -> Result<Vec<ServerSymb>, String> {
    Ok(server_stubs(e, symbs).iter()
       .map(|s|
            ServerSymb {
                name: String::from(s.name()),
                addr: s.addr()
            }
       ).collect())
}

pub struct ElfObject {
    obj_path: String,
    client_symbs: HashMap<String, ClientSymb>,
    server_symbs: HashMap<String, VAddr>,
    comp_symbs: CompSymbs
}

fn compute_elfobj(id: &ComponentId, obj_path: &String, s: &SystemState, b: &mut dyn BuildState) -> Result<Box<ElfObject>, String> {
    let obj_contents = dump_file(&obj_path)?;
    let obj = CompObject::parse(&obj_path, &obj_contents)?;

    let mut client_symbs = HashMap::new();
    let mut server_symbs = HashMap::new();

    for d in obj.dependencies().iter() {
        client_symbs.insert(d.name.clone(), ClientSymb {
            func_addr: d.func_addr,
            ucap_addr: d.ucap_addr
        });
    }

    for e in obj.exported().iter() {
        server_symbs.insert(e.name.clone(), e.addr);
    }

    Ok(Box::new(ElfObject {
        obj_path: obj_path.to_string(),
        client_symbs,
        server_symbs,
        comp_symbs: CompSymbs {
            entry: obj.entryfn_addr(),
            comp_info: obj.compinfo_addr()
        }
    }))
}


impl TransitionIter for ElfObject {
    fn transition_iter(id: &ComponentId, s: &SystemState, b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let obj_path = b.comp_build(&id, &s)?;

        compute_elfobj(&id, &obj_path, &s, b)
    }
}

impl ObjectsPass for ElfObject {
    fn client_symbs(&self) -> &HashMap<String, ClientSymb> {
        &self.client_symbs
    }

    fn server_symbs(&self) -> &HashMap<String, VAddr> {
        &self.server_symbs
    }

    fn comp_symbs(&self) -> &CompSymbs {
        &self.comp_symbs
    }

    fn comp_path(&self) -> &String {
        &self.obj_path
    }
}

pub struct Constructor {
    obj_path: String
}

impl Transition for Constructor {
    fn transition(s: &SystemState, b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let spec = s.get_spec();
        let mut sys_constructor = "".to_string();
        let constructors: Vec<&ComponentName> = spec
            .names()
            .iter()
            .rev()
            .map(|n| &spec.component_named(n).constructor)
            .filter(|c| c.var_name != "kernel")
            .unique()
            .collect();

        // Is rust smart enough to elide the actual collect here?
        // Because of the "return", I don't think so.
        for c_name in constructors.iter() {
            // simple reverse map on the id...
            let (id, _) = s
                .get_named()
                .ids()
                .iter()
                .find(|(id, name)| *name == *c_name)
                .unwrap();

            let obj_path = b.constructor_build(&id, &s)?;
            let obj = compute_elfobj(&id, &obj_path, &s, b)?;

            if obj.server_symbs() != s.get_objs_id(&id).server_symbs() {
                return Err(format!("Constructor {:?} creation error: Between when the object's synchronous invocations were generated, and when the constructor was synthesized, the code layout changed. This is an internal error, but we cannot proceed.", c_name));
            }

            if component(&s, &id).constructor.var_name == "kernel" {
                sys_constructor = obj_path;
            }
        }

        let img_path = b.file_path(&"cos.img".to_string())?;
        let cp_cmd = format!("cp {} {}", sys_constructor, img_path);
        let (out, err) = exec_pipeline(vec![cp_cmd.clone()]);
        if err.len() != 0 {
            return Err(format!("Errors copying image (in cmd {}):\n{}", cp_cmd, err));
        }

        Ok(Box::new(Constructor {
            obj_path: img_path
        }))
    }
}

impl ConstructorPass for Constructor {
    fn image_path(&self) -> &String {
        &self.obj_path
    }
}
