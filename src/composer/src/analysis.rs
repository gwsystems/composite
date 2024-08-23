use ascent::ascent;
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
const SERVER_PROPERTIES: &[(&'static str, CompProperties)] =
    &[("sched", Scheduler), ("capmgr", CapMgr), ("memmgr", MemMgr)];

// If a component is a client of one of these interfaces, then it is
// tagged with these properties.
const CLIENT_PROPERTIES: &[(&'static str, CompProperties)] = &[
    ("sched", MutExcl),  // TODO: should have a separate blkpt interface for this
    ("sched", CanBlock), // TODO: should have a separate blkpt interface for this
    ("memmgr", DynMem),
];

// Virtual resources have a type (here, a string), and the composer
// has assigned specific ids for pre-allocated resources.
type VirtResource = String;
type VirtResourceId = usize;

// Is a client allowed to read, modify, and/or dynamically allocate a
// virtual resource. No access only to Dyn as what is the point to
// allocating, but not having the ability to read/modify?
enum VirtResAccess {
    Read,        // read/access resource
    Modify,      // modify/write to resource
    DynAlloc,    // dynamically allocate resources
    Block,       // can a component block on the resource?
    Unspecified, // The interface is unlabeled (e.g. no virt resources)
}

pub struct AnalysisInput {
    // Each component, defined by id.
    components: Vec<(ComponentId)>,
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
    virt_resource_server: Vec<(ComponentId, VirtResource)>,
    // The access of clients to specific virtual resources
    virt_resource_access: Vec<(ComponentId, VirtResource, VirtResourceId, VirtResAccess)>,
    // Two virtual resources are associated with each other in a client component
    virt_resource_associated: Vec<(
        ComponentId,
        VirtResource,
        VirtResourceId,
        VirtResource,
        VirtResourceId,
    )>,
}

pub struct AnalysisOutput {
    // Simple transitive closure over the `dependencies` relation
    depends_on: Vec<(ComponentId, ComponentId)>,
    // All of the properties for each component
    comp_properties: Vec<(ComponentId, CompProperties)>,
    // The range of criticalities (min, max) for a specific component
    comp_crit_range: Vec<(ComponentId, usize, usize)>,
    // The first component can block the second.
    comp_can_block: Vec<(ComponentId, ComponentId)>,
    // Potential DOS on shared resources
    comp_dos: Vec<(ComponentId, ComponentId, ComponentId, VirtResource)>,
    // Lock interference is possible between the components
    comp_shared_lock: Vec<(ComponentId, ComponentId)>,
    // The maximum number of necessary threads for a component
    thread_limit: Vec<(ComponentId, usize)>,
    // What is the limit on the number of static virtual resources in
    // the server or client?
    virt_resource_static: Vec<(ComponentId, VirtResource, usize)>,
    // A server/manager component that requires dynamic allocation of
    // virtual resources.
    virt_resource_dynamic: Vec<(ComponentId, VirtResource)>,
}

ascent! {}

impl AnalysisOutput {
    fn new(i: &AnalysisInput) -> Self {}
}

struct Analysis {
    inputs: AnalysisInput,
    outputs: AnalysisOutput,
}

impl Analysis {
    fn build(s: &SystemState) -> Self {
        let inputs = AnalysisInput { .. };
        // Calculate the outputs
        let outputs = AnalysisOutput::new(&inputs);

        Analysis { inputs, outputs }
    }
}

impl Transition for Analysis {
    fn transition(s: &SystemState, b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let a = Analysis::build(s);

        Ok(Box::new(a))
    }
}
