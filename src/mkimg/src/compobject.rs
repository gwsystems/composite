use xmas_elf::{ElfFile, header, program};
use xmas_elf::sections;
use xmas_elf::sections::{SectionData};
use xmas_elf::symbol_table::{Entry, Entry32, DynEntry32, DynEntry64, Type, Binding};
use xmas_elf::sections::{SHF_ALLOC, SHF_COMPRESSED, SHF_WRITE, SHF_EXECINSTR, SHF_GROUP, SHF_INFO_LINK, SHF_LINK_ORDER, SHF_MASKOS, SHF_MASKPROC, SHF_MERGE, SHF_OS_NONCONFORMING, SHF_TLS, SHF_STRINGS};
use symbols::Symb;

pub struct ClientSymb {
    name: String,
    func_addr: u64,
    ucap_addr: u64
}

impl ClientSymb {
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

pub struct ServerSymb {
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

pub struct CompObject<'a> {
    name: String,
    object: ElfFile<'a>,
    dep_symbs: Vec<ClientSymb>,
    exp_symbs: Vec<ServerSymb>,
    compinfo_symb: u64,
    entryfn_symb: u64,
    defclifn_symb: u64,

}

impl<'a> CompObject<'a> {
    pub fn parse(name: String, obj:&'a Vec<u8>) -> Result<CompObject<'a>, String> {
        let elf_file = ElfFile::new(obj).unwrap();
        let symbs    = symbs_retrieve(&elf_file)?;

        let compinfo = compinfo_addr(&elf_file, symbs)?.addr();
        let entryfn  = entry_addr(&elf_file, symbs)?.addr();
        let defclifn = defcli_stub_addr(&elf_file, symbs)?.addr();

        let exps     = compute_exports(&elf_file, symbs)?;
        let deps     = compute_dependencies(&elf_file, symbs)?;

        Ok(CompObject {
            name: name,
            object: elf_file,
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

    pub fn dependencies(&self) -> &Vec<ClientSymb> {
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

fn compute_dependencies<'a>(e: &ElfFile<'a>, symbs: &'a [Entry32]) -> Result<Vec<ClientSymb>, String> {
    let defstub = defcli_stub_addr(e, symbs)?;
    let ucap_symbs = client_caps(e, symbs);
    let dep_symbs = client_stubs(e, symbs);

    Ok(ucap_symbs.iter()
       .map(|s| {
           let stub = dep_symbs.iter().fold(None, |found, next| {
               if next.name() == s.name() {
                   Some(next.addr())
               } else {
                   found
               }}).unwrap_or(defstub.addr());
           ClientSymb {
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

// fn section_symbols_print<'a>(e: &ElfFile<'a>, symbs: &[Entry32]) -> () {
//     client_caps(e, symbs).iter().for_each(|ref s| println!("{} @ {:x}", s.name(), s.addr()));
// }

// fn symbols_print(e: &ElfFile) {
//     let st = e.find_section_by_name(".symtab").unwrap();
//     match st.get_data(&e) {
//         //Ok(SectionData::DynSymbolTable32(sts)) => section_symbols_print(e, sts),
//         Ok(SectionData::SymbolTable32(sts)) => section_symbols_print(e, sts),
//         _ => ()
//     }
//     // let dst = e.find_section_by_name("dynsym").unwrap();
// }

// fn display_binary_information<P: AsRef<Path>>(binary_path: P) {
//     let buf = open_file(binary_path);
//     let elf_file = ElfFile::new(&buf).unwrap();
//     println!("{}", elf_file.header);
//     header::sanity_check(&elf_file).unwrap();

//     let mut sect_iter = elf_file.section_iter();
//     // Skip the first (dummy) section
//     sect_iter.next();
//     println!("sections");
//     for sect in sect_iter {
//         println!("{}", sect.get_name(&elf_file).unwrap());
//         println!("{:?}", sect.get_type());
//         //println!("{}", sect);
//         sections::sanity_check(sect, &elf_file).unwrap();

//         if sect.flags() & SHF_WRITE != 0 {
//             print!("SHF_WRITE, ");
//         }
//         if sect.flags() & SHF_ALLOC != 0 {
//             print!("SHF_ALLOC, ");
//         }
//         if sect.flags() & SHF_EXECINSTR != 0 {
//             print!("SHF_EXECINSTR, ");
//         }
//         if sect.flags() & SHF_MERGE != 0 {
//             print!("SHF_MERGE, ");
//         }
//         if sect.flags() & SHF_STRINGS != 0 {
//             print!("SHF_STRINGS, ");
//         }
//         if sect.flags() & SHF_INFO_LINK != 0 {
//             print!("SHF_INFO_LINK, ");
//         }
//         if sect.flags() & SHF_LINK_ORDER != 0 {
//             print!("SHF_LINK_ORDER, ");
//         }
//         if sect.flags() & SHF_OS_NONCONFORMING != 0 {
//             print!("SHF_OS_NONCONFORMING, ");
//         }
//         if sect.flags() & SHF_GROUP != 0 {
//             print!("SHF_GROUP, ");
//         }
//         if sect.flags() & SHF_TLS != 0 {
//             print!("SHF_TLS, ");
//         }
//         if sect.flags() & SHF_COMPRESSED != 0 {
//             print!("SHF_COMPRESSED, ");
//         }
//         if sect.flags() & SHF_MASKOS != 0 {
//             print!("SHF_MASKOS, ");
//         }
//         if sect.flags() & SHF_MASKPROC != 0 {
//             print!("SHF_MASKPROC, ");
//         }
//         println!("");

//         // if sect.get_type() == ShType::StrTab {
//         //     println!("{:?}", sect.get_data(&elf_file).to_strings().unwrap());
//         // }

//         // if sect.get_type() == ShType::SymTab {
//         //     if let sections::SectionData::SymbolTable64(data) = sect.get_data(&elf_file) {
//         //         for datum in data {
//         //             println!("{}", datum.get_name(&elf_file));
//         //         }
//         //     } else {
//         //         unreachable!();
//         //     }
//         // }
//     }

//     symbols_print(&elf_file);

//     let ph_iter = elf_file.program_iter();
//     println!("\nprogram headers");
//     for sect in ph_iter {
//         println!("{:?}", sect.get_type());
//         program::sanity_check(sect, &elf_file).unwrap();
//     }

//     match elf_file.program_header(0) {
//         Ok(sect) => {
//             println!("{}", sect);
//             match sect.get_data(&elf_file) {
//                 Ok(program::SegmentData::Note64(header, ptr)) => {
//                     println!("{}: {:?}", header.name(ptr), header.desc(ptr))
//                 }
//                 Ok(_) => (),
//                 Err(err) => println!("Error: {}", err),
//             }
//         }
//         Err(err) => println!("Error: {}", err),
//     }

//     // let sect = elf_file.find_section_by_name(".rodata.const2794").unwrap();
//     // println!("{}", sect);
// }
