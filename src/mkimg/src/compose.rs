use syshelpers::{dump_file, emit_file, reset_dir, exec_pipeline};
use cossystem::{CosSystem};
use compobject::CompObject;
use std::collections::BTreeMap;
use std::env;

pub struct ComposeSpec {
    sysspec: String,
    src_path: String,
    sys: CosSystem,
    binaries: BTreeMap<String, Vec<u8>>,
}

pub struct Compose<'a> {
    spec: &'a ComposeSpec,
    comp_objs: Vec<CompObject<'a>>
}

// Get the path to the component implementation directory. Should
// probably derive this from an environmental var passed in at compile
// time by the surrounding build system.
fn comps_base_path() -> String {
    let mut path = String::from(env!("PWD"));

    // Construct the path to the src directory
    path.push_str("/../");
    path.push_str("components/implementation/");
    path
}

// Get the path to a component object via its name.  <if>.<name>
// resolves to src/components/implementation/if/name/if.name.o
fn comp_path(comp_base: &String, img: &String) -> String {
    let mut obj_path = comp_base.clone();
    let mut obj_str = img.clone();
    let mut obj_subpath = obj_str.replace(".", "/");

    obj_subpath.push_str("/");
    obj_path.push_str(&obj_subpath);
    obj_path.push_str(&obj_str);
    obj_path.push_str(".o");
    obj_path
}

impl ComposeSpec {
    pub fn parse_spec(sysspec: String) -> Result<ComposeSpec, String> {
        let sys = CosSystem::parse(sysspec.clone())?;
        let mut bins = BTreeMap::new();
        let mut comps_path = comps_base_path();

        for c in sys.comps().iter() {
            let mut obj_path = comp_path(&comps_path, &c.img());
            let obj_contents = dump_file(&obj_path)?;

            bins.insert(c.name().clone(), obj_contents);
        }

        Ok(ComposeSpec {
            sysspec: sysspec,
            src_path: comps_path,
            sys: sys,
            binaries: bins,
        })
    }

    pub fn sysspec_output(&self) -> &CosSystem {
        &self.sys
    }
}

impl<'a> Compose<'a> {
    pub fn parse_binaries(spec: &'a ComposeSpec) -> Result<Compose<'a>, String> {
        let mut cs = Vec::new();

        for c in spec.sys.comps().iter() {
            let cbin = spec.binaries.get(c.name()).unwrap();
            cs.push(CompObject::parse(c.name().clone(), cbin)?);
        }

        Ok(Compose {
            spec: spec,
            comp_objs: cs
        })
    }

    pub fn components(&'a self) -> &'a Vec<CompObject<'a>> {
        &self.comp_objs
    }

    // sect_addrs is a vector of tuples of "section name", "address" pairs
    // fn relink(obj: String, sect_addrs: Vec<(String, String)>, tolink: Vec<String>) -> String {

    // }
}
