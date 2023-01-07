use xmas_elf::sections::SectionData;
use xmas_elf::symbol_table::{Binding, Entry, Type};
use xmas_elf::ElfFile;

use itertools::Itertools;
use passes::{
    component, BuildState, ClientSymb, CompSymbs, ComponentId, ComponentName, ConstructorPass,
    ObjectsPass, ServerSymb, SystemState, Transition, TransitionIter,
};
use std::collections::HashMap;
use symbols::{Symb, SymbType};
use syshelpers::{dump_file, exec_pipeline};

impl SymbType {
    fn new<'a>(symb: &'a dyn Entry) -> Self {
        match symb.get_binding() {
            Ok(Binding::Global) => match symb.get_type() {
                Ok(Type::NoType) | Ok(Type::Object) => SymbType::GlobalData,
                Ok(Type::Func) => SymbType::GlobalFn,
                _ => SymbType::Other,
            },
            _ => SymbType::Other,
        }
    }
}

struct ClientSymbol {
    name: String,
    func_addr: u64,
    callgate_addr: u64,
    ucap_addr: u64,
}

struct ServerSymbol {
    name: String,
    addr: u64,
    altfn_addr: u64,
}

struct CompObject {
    dep_symbs: Vec<ClientSymbol>,
    exp_symbs: Vec<ServerSymbol>,
    compinfo_symb: u64,
    entryfn_symb: u64,
}

impl CompObject {
    pub fn parse(_name: &String, obj: &Vec<u8>) -> Result<CompObject, String> {
        let elf_file = ElfFile::new(obj).unwrap();
        let symbs = symbs_retrieve(&elf_file)?;

        let compinfo = compinfo_addr(&symbs)?.addr();
        let entryfn = entry_addr(&symbs)?.addr();
        let _defclifn = defcli_stub_addr(&symbs)?.addr();

        let exps = compute_exports(&symbs)?;
        let deps = compute_dependencies(&symbs)?;

        Ok(CompObject {
            dep_symbs: deps,
            exp_symbs: exps,
            compinfo_symb: compinfo,
            entryfn_symb: entryfn,
        })
    }

    pub fn exported(&self) -> &Vec<ServerSymbol> {
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

fn symb_address<'a>(_e: &ElfFile<'a>, symb: &'a dyn Entry) -> u64 {
    symb.value()
}

fn global_variables<'a>(symbs: &Vec<Symb<'a>>) -> Vec<Symb<'a>> {
    symbs
        .iter()
        .filter_map(|s| match s.stype() {
            SymbType::GlobalData => Some(*s),
            _ => None,
        })
        .collect()
}

fn global_functions<'a>(symbs: &Vec<Symb<'a>>) -> Vec<Symb<'a>> {
    symbs
        .iter()
        .filter_map(|s| match s.stype() {
            SymbType::GlobalFn => Some(*s),
            _ => None,
        })
        .collect()
}

fn symbol_prefix_filter<'a>(
    symbs: &Vec<Symb<'a>>,
    prefix: &str,
    symb_filter: fn(&Vec<Symb<'a>>) -> Vec<Symb<'a>>,
) -> Vec<Symb<'a>> {
    symb_filter(symbs)
        .iter()
        .filter_map(|ref symb| {
            if symb.name().starts_with(prefix) {
                Symb::new(&symb.name()[prefix.len()..], symb.addr(), symb.stype())
            } else {
                None
            }
        })
        .collect()
}

fn client_caps<'a>(symbs: &Vec<Symb<'a>>) -> Vec<Symb<'a>> {
    symbol_prefix_filter(symbs, "__cosrt_ucap_", global_variables)
}

fn client_stubs<'a>(symbs: &Vec<Symb<'a>>) -> Vec<Symb<'a>> {
    symbol_prefix_filter(symbs, "__cosrt_c_", global_functions)
}

fn client_callgate_stubs<'a>(symbs: &Vec<Symb<'a>>) -> Vec<Symb<'a>> {
    symbol_prefix_filter(symbs, "__cosrt_fast_callgate_", global_functions)
}

fn server_stubs<'a>(symbs: &Vec<Symb<'a>>) -> Vec<Symb<'a>> {
    symbol_prefix_filter(symbs, "__cosrt_s_", global_functions)
}

fn server_alt_stubs<'a>(symbs: &Vec<Symb<'a>>) -> Vec<Symb<'a>> {
    symbol_prefix_filter(symbs, "__cosrt_alts_", global_functions)
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

fn compinfo_addr<'a>(symbs: &Vec<Symb<'a>>) -> Result<Symb<'a>, String> {
    unique_symbol(&mut symbol_prefix_filter(
        symbs,
        "__cosrt_comp_info",
        global_variables,
    ))
    .ok_or(String::from(
        "Could not find the __cosrt_upcall_entry function.",
    ))
}

fn entry_addr<'a>(symbs: &Vec<Symb<'a>>) -> Result<Symb<'a>, String> {
    unique_symbol(&mut symbol_prefix_filter(
        symbs,
        "__cosrt_upcall_entry",
        global_functions,
    ))
    .ok_or(String::from(
        "Could not find the __cosrt_upcall_entry function.",
    ))
}

fn defcli_stub_addr<'a>(symbs: &Vec<Symb<'a>>) -> Result<Symb<'a>, String> {
    unique_symbol(&mut symbol_prefix_filter(
        symbs,
        "__cosrt_c_cosrtdefault",
        global_functions,
    ))
    .ok_or(String::from(
        "Could not find the __cosrt_c_cosrtdefault function.",
    ))
}

fn symbs_retrieve<'a>(e: &ElfFile<'a>) -> Result<Vec<Symb<'a>>, String> {
    match e.find_section_by_name(".symtab").unwrap().get_data(&e) {
        //Ok(SectionData::DynSymbolTable32(sts)) => section_symbols_print(e, sts),
        Ok(SectionData::SymbolTable32(ref sts)) => Ok(sts
            .iter()
            .filter_map(|s| match s.get_name(&e) {
                Ok(n) => Symb::new(n, symb_address(&e, s), SymbType::new(s)),
                _ => None,
            })
            .collect()),
        Ok(SectionData::SymbolTable64(ref sts)) => Ok(sts
            .iter()
            .filter_map(|s| match s.get_name(&e) {
                Ok(n) => Symb::new(n, symb_address(&e, s), SymbType::new(s)),
                _ => None,
            })
            .collect()),
        _ => Err(String::from("Could not find the symbol table.")),
    }
}

fn compute_dependencies<'a>(symbs: &Vec<Symb<'a>>) -> Result<Vec<ClientSymbol>, String> {
    let defstub = defcli_stub_addr(symbs)?;
    let ucap_symbs = client_caps(symbs);
    let dep_symbs = client_stubs(symbs);
    let callgate_symbs = client_callgate_stubs(symbs);

    Ok(ucap_symbs
        .iter()
        .map(|s| {
            let stub = dep_symbs
                .iter()
                .fold(None, |found, next| {
                    if next.name() == s.name() {
                        Some(next.addr())
                    } else {
                        found
                    }
                })
                .unwrap_or(defstub.addr());
            let callgate_stub = callgate_symbs
                .iter()
                .fold(None, |found, next| {
                    if next.name() == s.name() {
                        Some(next.addr())
                    } else {
                        found
                    }
                })
                .unwrap_or(0);
            ClientSymbol {
                name: String::from(s.name()),
                func_addr: stub,
                callgate_addr: callgate_stub,
                ucap_addr: s.addr(),
            }
        })
        .collect())
}

fn compute_exports<'a>(symbs: &Vec<Symb<'a>>) -> Result<Vec<ServerSymbol>, String> {
    let alt_symbs = server_alt_stubs(symbs);

    Ok(server_stubs(symbs)
        .iter()
        .map(|s| {
            let altstub = alt_symbs
                .iter()
                .fold(None, |found, next|{
                    if next.name() == s.name() {
                        Some(next.addr())
                    } else {
                        found
                    }
                })
                .unwrap_or(0);
            ServerSymbol {
                name: String::from(s.name()),
                addr: s.addr(),
                altfn_addr: altstub,
            }
        })
        .collect())
}

pub struct ElfObject {
    obj_path: String,
    client_symbs: HashMap<String, ClientSymb>,
    server_symbs: HashMap<String, ServerSymb>,
    comp_symbs: CompSymbs,
}

fn compute_elfobj(
    _id: &ComponentId,
    obj_path: &String,
    _s: &SystemState,
    _b: &mut dyn BuildState,
) -> Result<Box<ElfObject>, String> {
    let obj_contents = dump_file(&obj_path)?;
    let obj = CompObject::parse(&obj_path, &obj_contents)?;

    let mut client_symbs = HashMap::new();
    let mut server_symbs = HashMap::new();

    for d in obj.dependencies().iter() {
        client_symbs.insert(
            d.name.clone(),
            ClientSymb {
                func_addr: d.func_addr,
                callgate_addr: d.callgate_addr,
                ucap_addr: d.ucap_addr,
            },
        );
    }

    for e in obj.exported().iter() {
        server_symbs.insert(
            e.name.clone(),
            ServerSymb {
                func_addr: e.addr,
                altfn_addr: e.altfn_addr,
            }
        );
    }

    Ok(Box::new(ElfObject {
        obj_path: obj_path.to_string(),
        client_symbs,
        server_symbs,
        comp_symbs: CompSymbs {
            entry: obj.entryfn_addr(),
            comp_info: obj.compinfo_addr(),
        },
    }))
}

impl TransitionIter for ElfObject {
    fn transition_iter(
        id: &ComponentId,
        s: &SystemState,
        b: &mut dyn BuildState,
    ) -> Result<Box<Self>, String> {
        let obj_path = b.comp_build(&id, &s)?;

        compute_elfobj(&id, &obj_path, &s, b)
    }
}

impl ObjectsPass for ElfObject {
    fn client_symbs(&self) -> &HashMap<String, ClientSymb> {
        &self.client_symbs
    }

    fn server_symbs(&self) -> &HashMap<String, ServerSymb> {
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
    obj_path: String,
}

impl Transition for Constructor {
    fn transition(s: &SystemState, b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let spec = s.get_spec();
        let mut sys_constructor = "".to_string();
        let constructors: Vec<&ComponentName> = spec
            .names()
            .iter()
            .rev()
            .map(|n| {
                let c = &spec.component_named(n);
                if c.constructor.var_name == "kernel" {
                    &c.name // the system constructor must be counted...even if it is the *only* component
                } else {
                    &c.constructor
                }
            })
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
                .find(|(_id, name)| *name == *c_name)
                .unwrap();

            let obj_path = b.constructor_build(&id, &s)?;
            let obj = compute_elfobj(&id, &obj_path, &s, b)?;

            for (name, symb) in obj.server_symbs().iter() {
                if symb.func_addr
                    != s.get_objs_id(&id)
                        .server_symbs()
                        .get(name)
                        .unwrap() // this really should not fail! How could the object have, then not have the symbol?
                        .func_addr
                {
                    return Err(format!("Constructor {:?} creation error: Between when the object's synchronous invocations were generated, and when the constructor was synthesized, the code layout changed. This is an internal error, but we cannot proceed.", c_name));
                }
            }

            if component(&s, &id).constructor.var_name == "kernel" {
                sys_constructor = obj_path;
            }
        }

        // If we didn't find the core system constructor, something is very wrong.
        if sys_constructor == "" {
            return Err(format!("Error: Could not find the system constructor with \"kernel\" as its own constructor. Error copying into the final constructor."));
        }

        let constructor_path = b.file_path(&"constructor".to_string())?;
        let cp_cmd = format!("cp {} {}", sys_constructor, constructor_path);
        let (_out, err) = exec_pipeline(vec![cp_cmd.clone()]);
        if err.len() != 0 {
            return Err(format!(
                "Errors copying image (in cmd {}):\n{}",
                cp_cmd, err
            ));
        }
        let kern_path = b.file_path(&"cos.img".to_string())?;
        b.kernel_build(&kern_path, &constructor_path, &s)?;

        Ok(Box::new(Constructor {
            obj_path: kern_path,
        }))
    }
}

impl ConstructorPass for Constructor {
    fn image_path(&self) -> &String {
        &self.obj_path
    }
}
