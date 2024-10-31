extern crate toml;
#[macro_use]
extern crate serde_derive;
extern crate itertools;
extern crate tar;
extern crate xmas_elf;
extern crate petgraph;
extern crate ascent;

mod address_assignment;
mod build;
mod compobject;
mod cossystem;
mod initargs;
mod invocations;
mod passes;
mod pipe;
mod properties;
mod resources;
mod symbols;
mod syshelpers;
mod tot_order;
mod virt_resources;
mod analysis;
mod graph;


use address_assignment::AddressAssignmentx86_64;
use build::DefaultBuilder;
use compobject::{Constructor, ElfObject};
use cossystem::SystemSpec;
use cossystem::ConstantVal;
use initargs::Parameters;
use invocations::Invocations;
use passes::{ BuildState, SInv, ComponentId, SystemState, Transition, TransitionIter};
use properties::CompProperties;
use resources::ResAssignPass;
use std::env;
use tot_order::CompTotOrd;
use virt_resources::VirtResAnalysis;
use graph::Graph;
use analysis::Analysis;

pub fn exec() -> Result<(), String> {
    let mut args = env::args();
    let program_name = args.next();

    let arg1 = args.next();
    let arg2 = args.next();
    let arg3 = args.next();

    if None == arg1 || None == arg2 {
        return Err(format!(
            "usage: {} <sysspec>.toml <buildname>",
            program_name.unwrap()
        ));
    }

    let is_rebuild = match arg3 {
        Some(ref val) if val == "REBUILD" => true,
        Some(_) => return Err(format!("Invalid third argument. Expected 'REBUILD'.")),
        None => false,
    };

    let mut sys = SystemState::new(arg1.unwrap());
    let mut build = DefaultBuilder::new();
    build.initialize(&arg2.unwrap(), false, &sys)?;


    sys.add_parsed(SystemSpec::transition(&sys, &mut build)?);
    sys.add_named(CompTotOrd::transition(&sys, &mut build)?);
    sys.add_virt_res(VirtResAnalysis::transition(&sys, &mut build)?);
    sys.add_address_assign(AddressAssignmentx86_64::transition(&sys, &mut build)?);
    sys.add_properties(CompProperties::transition(&sys, &mut build)?);
    sys.add_restbls(ResAssignPass::transition(&sys, &mut build)?);

    // process these in reverse order of dependencies (e.g. booter last)
    let reverse_ids: Vec<ComponentId> = sys
        .get_named()
        .ids()
        .iter()
        .map(|(k, _)| k.clone())
        .rev()
        .collect();
    for c_id in reverse_ids.iter() {
        let header_file_path =
            build.comp_file_path(&c_id, &"component_constants.h".to_string(), &sys)?;
        build.comp_init_header_file(&header_file_path);

        sys.add_params_iter(&c_id, Parameters::transition_iter(c_id, &sys, &mut build)?);
        sys.add_objs_iter(&c_id, ElfObject::transition_iter(c_id, &sys, &mut build)?);
        sys.add_invs_iter(&c_id, Invocations::transition_iter(c_id, &sys, &mut build)?);
    }
    sys.add_constructor(Constructor::transition(&sys, &mut build)?);
    sys.add_graph(Graph::transition(&sys, &mut build)?);
    sys.add_analysis(Analysis::transition(&sys, &mut build)?);
  
    let analysis = sys.get_analysis();  
    let component_ids = sys.get_named().ids();

    for (comp_id, _) in component_ids {
        let warnings_str = analysis.warning_str(*comp_id, &sys); // Get the warnings for the component
        println!("{}", warnings_str); // Print the warnings
    }

    /*1.iterate the components
      2.get the invocation function 
      3.add the entry prefix 
      4.call the stack size analysis 
      5.return the stack size
      6.insert stack size into header file
      6.TBD: get the analysis result of thread number
      7.insert the max_local_num_thread into header file*/ 
    let booter_id = 1;
    for c_id in reverse_ids.iter() {
        let invocations_pass = sys.get_invs_id(&booter_id);
        let invs = invocations_pass.invocations();
        let mut iner_constants = Vec::new();

        let filtered_invs: Vec<&SInv> = invs
        .iter()
        .filter(|inv| inv.server == *c_id)
        .collect();

        let mut symbol_names: Vec<String> = filtered_invs
        .iter()
        .map(|inv| format!("__cosrt_s_{}", inv.symb_name))
        .collect();

        symbol_names.push("__cosrt_upcall_entry".to_string());

        /* call your stack size analysis tool here get the result */
        let stack_size = "replace with your tool";

        println!("invs name for component {} at: {:?}", &c_id, &invs);

        println!("filtered_invs name for component {} at: {:?}",&c_id, &filtered_invs);

        println!("symbol name for component {} at: {:?}",&c_id, &symbol_names);

        let new_constant = ConstantVal {
            variable: "COS_STACK_SZ".to_string(),
            value: stack_size.to_string(),
        };

        iner_constants.push(new_constant);

        let header_file_path =
            build.comp_file_path(&c_id, &"component_constants.h".to_string(), &sys)?;

        build.comp_const_header_file(&header_file_path, Some(iner_constants), &c_id, &sys)?;

        /*rebuild this system */
        build.set_rebuild_flag(is_rebuild);
        sys.add_objs_iter(&c_id, ElfObject::transition_iter(c_id, &sys, &mut build)?);
    }

    println!(
        "System object generated:\n\t{}",
        sys.get_constructor().image_path()
    );

    Ok(())
}

pub fn main() -> () {
    if let Err(e) = exec() {
        println!("{}", e);
    }
}
