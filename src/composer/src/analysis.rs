use ascent::aggregators::{max, min};
use ascent::{ascent_run, lattice::Dual};
use passes::{
    component, BuildState, ComponentId, ComponentName, Dependency, GraphPass, Interface,
    ServiceType, SystemState, Transition, Variant,
};
use std::collections::HashMap;
use std::fmt;

// A very simplistic criticality-level specification.
type CriticalityLvl = usize;
const HC: CriticalityLvl = 1;
const LC: CriticalityLvl = 0;
const NUM_CRIT: usize = 2;

// Component properties that are useful in determining how they impact
// other components.
#[derive(Clone, PartialEq, Eq, Hash, Copy)]
enum CompProperties {
    Scheduler,
    CapMgr,
    MemMgr,
    Constructor,
    MutExcl,
    DynMem,
    CanBlock,
}

// The mapping between interfaces exported by a server, and the
// properties of that server.
//
// This is hard-coded for now, but should be configured somewhere more
// reasonable.
const SERVER_PROPERTIES: &[(&'static str, CompProperties)] = &[
    ("sched", CompProperties::Scheduler),
    ("capmgr", CompProperties::CapMgr),
    ("memmgr", CompProperties::MemMgr),
];

// If a component is a client of one of these interfaces, then it is
// tagged with these properties.
//
// This is hard-coded for now, but should be configured somewhere more
// reasonable.
const CLIENT_PROPERTIES: &[(&'static str, CompProperties)] = &[
    ("sched", CompProperties::MutExcl), // TODO: should have a separate blkpt interface for this
    ("sched", CompProperties::CanBlock), // TODO: should have a separate blkpt interface for this
    ("memmgr", CompProperties::DynMem),
];

// Virtual resources have a type (here, a string), and the composer
// has assigned specific ids for pre-allocated resources.
type VirtResource = String;
type VirtResourceId = usize;

// Is a client allowed to read, modify, and/or dynamically allocate a
// virtual resource. No access only to Dyn as what is the point to
// allocating, but not having the ability to read/modify?
#[derive(Clone, PartialEq, Eq, Hash)]
enum VirtResAccess {
    Read,        // read/access resource
    Modify,      // modify/write to resource
    DynAlloc,    // dynamically allocate resources
    Block,       // can a component block on the resource?
    Unspecified, // The interface is unlabeled (e.g. no virt resources)
}

pub struct AnalysisInput {
    components: Vec<ComponentId>,
    // What is the criticality level of each component. Some of these
    // (a minority) will be input, and the analysis will calculate the
    // criticality ranges (`comp_crit_range`) for shared components.
    criticalities: Vec<(ComponentId, CriticalityLvl)>,
    // Dependencies between components (client -> server), based on
    // interfaces, augmented with which if the dependency on that
    // interface includes functions that imbue specific virtual
    // resource access rights.
    dependencies: Vec<(ComponentId, ComponentId, Interface, VirtResAccess)>,
    // Virtual resources, and the server that provides them.
    // virt_resource_server: Vec<(ComponentId, VirtResource)>,
    // The access of clients to specific virtual resources
    // virt_resource_access: Vec<(ComponentId, VirtResource, VirtResourceId, VirtResAccess)>,
    // Two virtual resources are associated with each other in a client component
    // virt_resource_associated: Vec<(
    //     ComponentId,
    //     VirtResource,
    //     VirtResourceId,
    //     VirtResource,
    //     VirtResourceId,
    // )>,
}

pub struct Analysis {
    // Simple transitive closure over the `dependencies` relation for
    // (client, server)
    depends_on: Vec<(ComponentId, ComponentId)>,
    // All of the properties for each component
    comp_properties: Vec<(ComponentId, CompProperties)>,
    // The range of criticalities (min, max) for a specific component
    comp_crit_range: Vec<(ComponentId, usize, Dual<usize>)>,
    // The first component can block the second.
    comp_can_block: Vec<(ComponentId, ComponentId)>,
    // Potential DOS on shared resources in server, from client
    comp_dos: Vec<(ComponentId, ComponentId, VirtResource)>,
    // Lock interference is possible between the components
    comp_shared_lock: Vec<(ComponentId, ComponentId)>,
    // synchronous invocations while holding a lock can generate
    // nested locking. This is the set of potential nested locks. Can
    // take the server, while in a synchronous invocation from the
    // client.
    comp_nested_lock: Vec<(ComponentId, ComponentId)>,
    // The maximum number of necessary threads for a component
    //thread_limit: Vec<(ComponentId, usize)>,
    // What is the limit on the number of static virtual resources in
    // the server or client?
    // virt_resource_static: Vec<(ComponentId, VirtResource, usize)>,
    // A server/manager component that requires dynamic allocation of
    // virtual resources.
    // virt_resource_dynamic: Vec<(ComponentId, VirtResource)>,
}

fn analysis_output(i: AnalysisInput) -> Analysis {
    let srv_prop_map: Vec<(String, CompProperties)> = SERVER_PROPERTIES
        .iter()
        .map(|(i, p)| (i.to_string(), *p))
        .collect();
    let cli_prop_map: Vec<(String, CompProperties)> = CLIENT_PROPERTIES
        .iter()
        .map(|(i, p)| (i.to_string(), *p))
        .collect();

    // Destructure, so that we can individually move the vectors
    // into the program.
    let AnalysisInput {
	components,
	criticalities,
	dependencies,
    } = i;

    let p = ascent_run! {
        // inputs
        relation criticalities(ComponentId, CriticalityLvl) = criticalities;
        relation dependencies(ComponentId, ComponentId, Interface, VirtResAccess) = dependencies;
        relation srv_prop_map(String, CompProperties) = srv_prop_map;
        relation cli_prop_map(String, CompProperties) = cli_prop_map;

        // transient relations
        relation criticality_exposure(ComponentId, CriticalityLvl);
	lattice comp_crit_hi(ComponentId, CriticalityLvl);
	lattice comp_crit_lo(ComponentId, Dual<CriticalityLvl>);

        // outputs
        relation depends_on(ComponentId, ComponentId);
        relation comp_properties(ComponentId, CompProperties);
        relation comp_crit_range(ComponentId, usize, Dual<usize>);
        relation comp_can_block(ComponentId, ComponentId);
        relation comp_dos(ComponentId, ComponentId, VirtResource);
        relation comp_shared_lock(ComponentId, ComponentId);
        relation comp_nested_lock(ComponentId, ComponentId);

        // Normal transitive closure
        depends_on(c, s) <-- dependencies(c, s, _, _);
        depends_on(c, ss) <-- depends_on(c, s), dependencies(s, ss, _, _);

        // server properties
        comp_properties(s, p) <-- dependencies(_, s, i, _), srv_prop_map(i, p);
        // client properties
        comp_properties(c, p) <-- dependencies(c, _, i, _), cli_prop_map(i, p);

        // Compute the criticality exposure for each component
        criticality_exposure(c, crit) <-- criticalities(c, crit);
        criticality_exposure(s, crit) <-- depends_on(c, s), criticalities(c, crit);
        comp_crit_hi(c, crit) <-- criticality_exposure(c, ?crit);
        comp_crit_lo(c, Dual(*crit)) <-- criticality_exposure(c, ?crit);
        comp_crit_range(c, hi, Dual(*lo)) <-- comp_crit_hi(c, ?hi), comp_crit_lo(c, ?Dual(lo));

	// TODO: take into account blocking on shared virtual resources
	comp_can_block(s, c) <-- comp_properties(c, CompProperties::CanBlock);

        comp_shared_lock(s, c) <-- comp_properties(s, CompProperties::MutExcl), depends_on(c, s);

        comp_nested_lock(s, c) <-- comp_properties(s, CompProperties::MutExcl), comp_properties(c, CompProperties::MutExcl), depends_on(c, s);
    };

    Analysis {
        depends_on: p.depends_on,
        comp_properties: p.comp_properties,
        comp_crit_range: p.comp_crit_range,
        comp_can_block: p.comp_can_block,
        comp_dos: p.comp_dos,
        comp_shared_lock: p.comp_shared_lock,
        comp_nested_lock: p.comp_nested_lock,
    }
}

impl Analysis {
    fn build(s: &SystemState) -> Self {
        let cs = s.get_spec();
        let ids = s.get_named();

        let compid = |c| ids.rmap().get(c).unwrap();
        let components: Vec<ComponentId> = cs.names().iter().map(|c| *compid(c)).collect();
        // TODO: get the criticalities from the spec
        let criticalities: Vec<(ComponentId, CriticalityLvl)> =
            components.iter().map(|c| (*c, LC)).collect();
        let dependencies: Vec<(ComponentId, ComponentId, Interface, VirtResAccess)> = cs
            .names()
            .iter()
            .map(|c| {
                cs.deps_named(c)
                    .iter()
                    .filter_map(|d| {
                        if d.variant != "kernel" {
                            Some((
                                *compid(c),
                                *compid(&d.server),
                                d.interface.clone(),
                                VirtResAccess::Unspecified,
                            ))
                        } else {
                            None
                        }
                    })
                    .collect::<Vec<_>>()
            })
            .flatten()
            .collect();

        // Calculate the analysis outputs
        analysis_output(AnalysisInput {
            components,
            dependencies,
            criticalities,
        })
    }

    fn comp_crit_range(&self, c: &ComponentId) -> (CriticalityLvl, CriticalityLvl) {
	let (h, l) = self.comp_crit_range.iter().filter_map(|(id, h, l)| if c == id {
	    Some((h, l))
	} else {
	    None
	}).take(1).next().unwrap();

	// First deref on `l` is to get past the Dual
	(*h, **l)
    }

    fn comp_can_block(&self, c: &ComponentId) -> bool {
	: p.comp_can_block,
        comp_dos: p.comp_dos,
        comp_shared_lock: p.comp_shared_lock,
        comp_nested_lock: p.comp_nested_lock,

}

impl Transition for Analysis {
    fn transition(s: &SystemState, b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let a = Analysis::build(s);

        Ok(Box::new(a))
    }
}
