extern crate toml;
extern crate pipers;
#[macro_use]
extern crate serde_derive;
extern crate xmas_elf;
extern crate tar;
extern crate itertools;

mod cossystem;
mod syshelpers;
mod compobject;
mod symbols;
mod invocations;
mod build;
mod initargs;
mod resources;
mod passes;
mod tot_order;

use std::env;
use passes::{SystemState, Transition, TransitionIter, BuildState, ComponentId};
use build::{DefaultBuilder};
use cossystem::{SystemSpec};
use tot_order::{CompTotOrd};
use resources::{ResAssignPass};
use initargs::{Parameters};
use compobject::{ElfObject, Constructor};
use invocations::{Invocations};

pub fn exec() -> Result<(), String> {
    let mut args = env::args();
    let program_name = args.next();

    let arg1 = args.next();
    let arg2 = args.next();

    if None == arg1 || None == arg2 {
        return Err(format!("usage: {} <sysspec>.toml <buildname>", program_name.unwrap()));
    }

    let mut sys = SystemState::new(arg1.unwrap());
    let mut build = DefaultBuilder::new();
    build.initialize(&arg2.unwrap(), &sys)?;

    sys.add_parsed(SystemSpec::transition(&sys, &mut build)?);
    sys.add_named(CompTotOrd::transition(&sys, &mut build)?);
    sys.add_restbls(ResAssignPass::transition(&sys, &mut build)?);

    let reverse_ids: Vec<ComponentId> = sys.get_named().ids().iter().map(|(k, v)| k.clone()).rev().collect();
    for c_id in reverse_ids.iter() {
        sys.add_params_iter(&c_id, Parameters::transition_iter(c_id, &sys, &mut build)?);
        sys.add_objs_iter(&c_id, ElfObject::transition_iter(c_id, &sys, &mut build)?);
        sys.add_invs_iter(&c_id, Invocations::transition_iter(c_id, &sys, &mut build)?);
    }
    sys.add_constructor(Constructor::transition(&sys, &mut build)?);

    println!("Final object in {}", sys.get_constructor().image_path());

    Ok(())

    // match ComposeSpec::parse_spec(arg1.unwrap()) {
    //     Ok(sysspec) => {
    //         println!("System Specification:\n{:#?}", sysspec.sysspec_output());
    //         match Compose::parse_binaries(sysspec) {
    //             Ok(sys) => {
    //                 sys.components().iter().for_each(|(s, ref c)| comp_print(&c))
    //             },
    //             Err(s) => println!("{}", s)
    //         }
    //     },
    //     Err(s) => println!("{}", s)
    // }
}

pub fn main() -> () {
    if let Err(e) = exec() {
        println!("{}", e);
    }
}
