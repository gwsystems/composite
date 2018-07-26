// This file gathers together all of the information from:
//
// 1. the system specification,
// 2. a processing of that specification that infers all of the data
//     not provided by the specification itself
// 3. the component objects themselves post-linking
//
// It orchestrates the linking of all components, and derives all of
// the information needed by the booter to link the system graph
// together (e.g. all of the sinv information).
//
// It has to work around an anoying Rust issue: We need to read in the
// binaries for each component, then process them as Elf binaries.
// However, the entire data-structure cannot be completed at once
// since it involved dynamic creation via the system specification.
// Thus the explicit lifetime of the binaries prevents the Elf
// references being created until all objects have been read in.
// Essentially, we need a reference within the structure to itself,
// but the explicit lifetimes (necessary for the Elf library) prevent
// this. Thus, this is broken into two structures across two phases of
// processing, the first generating the ComposeSpec structure based on
// specification processing, and Compose which performs the binary
// processing and resulting computation.


use syshelpers::{dump_file, emit_file, reset_dir, exec_pipeline};
use cossystem::{CosSystem};
use compobject::CompObject;
use build::{BuildContext,comps_base_path, interface_path, comp_path};
use std::collections::BTreeMap;
use std::env;

pub struct ComposeSpec {
    sysspec: String,
    src_path: String,
    sys: CosSystem,
    binaries: BTreeMap<String, Vec<u8>>,
    build_ctxt: BuildContext
}

// All of the information required to create all of the synchronous
// invocations between a client and server for a single function
#[derive(Debug)]
pub struct Sinv {
    name: String,
    client: String,
    server: String,
    c_fn_addr: u64,
    c_ucap_addr: u64,
    s_fn_addr: u64
}

pub struct Compose<'a> {
    spec: &'a ComposeSpec,
    comp_objs: BTreeMap<String, CompObject<'a>>,
    sinvs: Vec<Sinv>
}

impl ComposeSpec {
    pub fn parse_spec(sysspec: String) -> Result<ComposeSpec, String> {
        let sys = CosSystem::parse(sysspec.clone())?;
        let mut bins = BTreeMap::new();
        let mut comps_path = comps_base_path();
        let mut build_ctxt = BuildContext::new(&sys.comps());

        if let Err(s) = build_ctxt.validate_deps() {
            return Err(s);
        }
        build_ctxt.build_components();

        for c in sys.comps().iter() {
            let obj_path = build_ctxt.comp_obj_path(c.name()).unwrap();
            let obj_contents = dump_file(&obj_path)?;

            bins.insert(c.name().clone(), obj_contents);
        }

        Ok(ComposeSpec {
            sysspec: sysspec,
            src_path: comps_path,
            sys: sys,
            binaries: bins,
            build_ctxt: build_ctxt
        })
    }

    pub fn sysspec_output(&self) -> &CosSystem {
        &self.sys
    }
}

impl<'a> Compose<'a> {
    pub fn parse_binaries(spec: &'a ComposeSpec) -> Result<Compose<'a>, String> {
        let mut cs = BTreeMap::new();

        for c in spec.sys.comps().iter() {
            cs.insert(c.name().clone(), CompObject::parse(c.name().clone(), spec.binaries.get(c.name()).unwrap())?);
        }

        // We have completed all dependencies, and now have the
        // objects for symbol processing.  Time to create the sinv
        // dependencies.
        let mut sinvs = Vec::new();
        for (n, c) in cs.iter() {
            let cli = cs.get(n).unwrap();
            for (s, i) in spec.build_ctxt.comp_deps(n).unwrap().iter() {
                let srv = cs.get(s).unwrap();
                let mut cnt = 0;

                for dep in cli.dependencies().iter() {
                    for exp in srv.exported().iter() {
                        if dep.name() == exp.name() {
                            sinvs.push(Sinv {
                                name: dep.name().clone(),
                                client: n.clone(),
                                server: s.clone(),
                                c_fn_addr: dep.func_addr(),
                                c_ucap_addr: dep.ucap_addr(),
                                s_fn_addr: exp.addr()
                            });
                            cnt = cnt + 1;
                            break;
                        }
                    }
                }
                // We had better have satisified all of the
                // dependencies!  We should have essentially validated
                // this through the component linking phase
                // (i.e. there should have been compiler errors there
                // if this weren't true, so this really is an internal
                // data-structure/consistency problem for this
                // program.
                assert!(cli.dependencies().len() == cnt);
            }
        }

        Ok(Compose {
            spec: spec,
            comp_objs: cs,
            sinvs: sinvs
        })
    }

    pub fn components(&'a self) -> &'a BTreeMap<String, CompObject<'a>> {
        &self.comp_objs
    }

    pub fn sinvs(&'a self) -> &'a Vec<Sinv> {
        &self.sinvs
    }

    // sect_addrs is a vector of tuples of "section name", "address" pairs
    // fn relink(obj: String, sect_addrs: Vec<(String, String)>, tolink: Vec<String>) -> String {

    // }
}
