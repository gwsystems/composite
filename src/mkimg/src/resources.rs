use passes::{BuildState, CapTable, ComponentId, ResPass, SystemState, Transition};
use std::collections::{BTreeMap, HashMap};

pub type CoreNum = u32;

// resources allocated in slices
pub struct RateSlice {
    slice: u32,
    prio: u32,
}

// resources allocated over finite or infinite window of time
pub enum RateWindow {
    Window(u32),
    Inf,
}

// For each core, what temporal partitions are required?
pub struct CPUPartition {
    window: RateWindow,
    slices: HashMap<u32, RateSlice>, // component -> allocation
}

pub struct Rate {
    slice: RateSlice,
    window: RateWindow,
}

pub struct SchedParam {
    alloc: Option<Rate>,
}

// The assignment of components to cores. Which cores are they allowed
// to create exectution on? Note that this information also needs to
// get passed to the components themselves.
pub struct CoreAssignment {
    cores: HashMap<CoreNum, SchedParam>,
}

// Memory is allocated either as a finite span of memory, or as "the
// rest" to consume remaining system memory. This is the user-level
// memory, and the capability manager is in charge of managing
// untyped/kernel memory.
pub enum MemAmnt {
    Span(u32), // should be a multiple of PAGE_SIZE
    Remaining,
}

// The mkimg representation of the system's resources, and which
// components are in charge of which resources for which other
// components.
pub enum Resource {
    // Initial execution set of components for which this component
    // provides bootup services. This requires the component to
    // statically know about the component and access/create to
    // component, page-tables, and capability tables.
    Bootup(Vec<ComponentId>),
    // This component is responsible for scheduling the specified set
    // of components. Specifically, this includes the execution of
    // cos_init and the synchronization of execution in order of
    // dependencies, and after cos_init executes for each, this
    // includes the scheduling of the main/other thread execution.
    // This requires scheduling and all of its inherent
    // complexity. The scheduler requires the ability to make threads,
    // tcaps, snd/arcv end-points in other components (directly or
    // though another component), and direct access to the thread
    // capabilities. These are also determined by the "init"
    // interface.
    Sched(HashMap<ComponentId, CoreAssignment>),
    // Static partitioning of CPU. Set of components to which this
    // component provides static execution rate allocations using
    // tcaps. This represents temporal partitioning. We assume that a
    // single window over which allocations are made is shared by all
    // components. This could be generalized by ensuring separate
    // windows have a reasonable least common multiple. This is
    // determined entirely by the specification in the runscript, and
    // this component must be responsible for initial execution.
    ExecPartition(HashMap<CoreNum, CPUPartition>),
    // The component provides dynamic memory allocation and management
    // to the set of components. This requires that it have access to
    // their page-tables (at least indirectly), and know the set of
    // components to which it is providing memory servies. This is
    // determined by the "mem" interface.
    DynMem(HashMap<ComponentId, MemAmnt>),
    // Static memory partitions this component should give to
    // other components. These are strong partitions that split the
    // system's physical memory between the partitions. They remove
    // shared memory opportunities, unless one of the partitions
    // coordinates that for the others. This requires an understanding
    // of which components it should provide memory, and how much. It
    // requires access to their page-tables, and, as we're using the
    // boot-time capability-table format, access to the capability
    // table as well. For now, this partitioning is only of user-typed
    // memory. This is determined by the runscript.
    MemPartition(HashMap<ComponentId, MemAmnt>),
    // The set of components for which this component provides
    // capability management. This component is required to provide
    // the ability to allocate different system resources for the set
    // of other components which means it has to have access to their
    // capability tables, and untyped/kernel memory. This is
    // determined by the "cap" interface.
    Cap(Vec<ComponentId>),
}

// resource, and parent of the resource pairs
pub struct Res {
    resource: Resource,
    parent: Option<ComponentId>,
}

pub struct ResAssignPass {
    resources: HashMap<ComponentId, CapTable>,
}

impl Transition for ResAssignPass {
    fn transition(s: &SystemState, b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let mut res = HashMap::new();
        let ids = s.get_named().ids();

        for (k, v) in ids.iter() {
            res.insert(k.clone(), BTreeMap::new());
        }

        Ok(Box::new(ResAssignPass { resources: res }))
    }
}

impl ResPass for ResAssignPass {
    fn cap_tbl(&self, id: &ComponentId) -> &CapTable {
        self.resources.get(&id).unwrap()
    }
}
