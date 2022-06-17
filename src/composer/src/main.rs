extern crate pipers;
extern crate toml;
#[macro_use]
extern crate serde_derive;
extern crate itertools;
extern crate tar;
extern crate xmas_elf;

mod build;
mod compobject;
mod cossystem;
mod initargs;
mod invocations;
mod passes;
mod resources;
mod symbols;
mod syshelpers;
mod tot_order;
mod properties;
mod address_assignment;

use build::DefaultBuilder;
use compobject::{Constructor, ElfObject};
use cossystem::SystemSpec;
use initargs::Parameters;
use invocations::Invocations;
use passes::{BuildState, ComponentId, SystemState, Transition, TransitionIter};
use resources::ResAssignPass;
use properties::CompProperties;
use std::env;
use tot_order::CompTotOrd;
use address_assignment::AddressAssignmentx86_64;

pub fn exec() -> Result<(), String> {
    let mut args = env::args();
    let program_name = args.next();

    let arg1 = args.next();
    let arg2 = args.next();

    if None == arg1 || None == arg2 {
        return Err(format!(
            "usage: {} <sysspec>.toml <buildname>",
            program_name.unwrap()
        ));
    }

    let mut sys = SystemState::new(arg1.unwrap());
    let mut build = DefaultBuilder::new();
    build.initialize(&arg2.unwrap(), &sys)?;

    sys.add_parsed(SystemSpec::transition(&sys, &mut build)?);
    sys.add_named(CompTotOrd::transition(&sys, &mut build)?);
    sys.add_address_assign(AddressAssignmentx86_64::transition(&sys, &mut build)?);
    sys.add_properties(CompProperties::transition(&sys, &mut build)?);
    sys.add_restbls(ResAssignPass::transition(&sys, &mut build)?);
    sys.add_repos(RepoPass::transition(&sys, &mut build)?);

    // process these in reverse order of dependencies (e.g. booter last)
    let reverse_ids: Vec<ComponentId> = sys
        .get_named()
        .ids()
        .iter()
        .map(|(k, _)| k.clone())
        .rev()
        .collect();
    for c_id in reverse_ids.iter() {
        sys.add_params_iter(&c_id, Parameters::transition_iter(c_id, &sys, &mut build)?);
        sys.add_objs_iter(&c_id, ElfObject::transition_iter(c_id, &sys, &mut build)?);
        sys.add_invs_iter(&c_id, Invocations::transition_iter(c_id, &sys, &mut build)?);
    }
    sys.add_constructor(Constructor::transition(&sys, &mut build)?);

    println!("System object generated:\n\t{}", sys.get_constructor().image_path());

    Ok(())
}

pub fn main() -> () {
    if let Err(e) = exec() {
        println!("{}", e);
    }
}
