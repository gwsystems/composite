use initargs::ArgsKV;
use passes::{
    component, deps, BuildState, ComponentId, Interface, PropertiesPass, ResPass, ServiceType,
    SystemState, Transition,
};
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

// Resource must be allocated and delegated to populate the resource
// tables for each component, and determine which is in charge of
// which resources.
#[derive(Debug)]
enum CapRes {
    CapTbl(ComponentId),
    PgTbl(ComponentId),
    Comp(ComponentId),
    Thd(ComponentId),
    SInv, // only used to lookup its size
    TCap(ComponentId),
    Rcv(ComponentId), // ...
}

// The equivalent of the C __captbl_cap2sz(c)
fn cap_sz(cap: &CapRes) -> u32 {
    match cap {
        CapRes::Thd(_) | CapRes::TCap(_) => 1,
        CapRes::CapTbl(_) | CapRes::PgTbl(_) => 2,
        CapRes::Rcv(_) | CapRes::SInv | CapRes::Comp(_) => 4,
    }
}

// returns the size of the capability, the type (in captbl, pgtbl,
// comp, for now), the target, and the indirect target.
fn cap_info(c: &CapRes) -> (String, ComponentId) {
    let (n, id) = match c {
        CapRes::CapTbl(id) => ("captbl".to_string(), id),
        CapRes::PgTbl(id) => ("pgtbl".to_string(), id),
        CapRes::Comp(id) => ("comp".to_string(), id),
        _ => unimplemented!("Capabilities other than pgtbl, captbl, and component not supported"),
    };

    (n, *id)
}

struct CaptblState {
    captbl: BTreeMap<u32, CapRes>,
    frontier: u32,
    prev_sz: u32,
}

impl CaptblState {
    fn new() -> CaptblState {
        CaptblState {
            captbl: BTreeMap::new(),
            frontier: 44, // BOOT_CAPTBL_FREE...FIXME: automatically generate this
            prev_sz: 4,
        }
    }

    fn add(&mut self, cap: CapRes) {
        let sz = cap_sz(&cap);
        let frontier = self.frontier;

        if (frontier % 4 != 0) && (self.prev_sz != sz) {
            self.frontier = frontier + (4 - frontier % 4); //  round up to power of 4
        }
        self.frontier += sz;
        self.prev_sz = sz;

        self.captbl.insert(frontier, cap);
    }

    fn get_captbl(&self) -> &BTreeMap<u32, CapRes> {
        &self.captbl
    }

    fn get_frontier(&self) -> u32 {
        self.frontier.clone()
    }
}

struct CompConfigState {
    ct: CaptblState,
    args: Vec<ArgsKV>,
}

impl CompConfigState {
    fn new() -> CompConfigState {
        CompConfigState {
            ct: CaptblState::new(),
            args: Vec::new(),
        }
    }

    fn get_args(&self) -> &Vec<ArgsKV> {
        &self.args
    }
}

fn comp_config(s: &SystemState, id: &ComponentId, cfg: &mut CompConfigState) {}

fn sched_config_serv_client(s: &SystemState, id: &ComponentId) -> Vec<ArgsKV> {
    let mut init = Vec::new();

    let props: &PropertiesPass = s.get_properties();
    if !props.service_is_a(&id, ServiceType::Scheduler) {
        return init;
    }

    let clients = props.service_clients(&id, ServiceType::Scheduler);

    assert!(props.service_is_a(&id, ServiceType::Scheduler));
    if let Some(cs) = clients {
        for c in cs.iter() {
            init.push(ArgsKV::new_key(c.to_string(), id.to_string()));
        }
    }

    init
}

fn sched_config_clients(s: &SystemState, id: &ComponentId) -> Vec<ArgsKV> {
    let mut init = Vec::new();

    let props: &PropertiesPass = s.get_properties();
    if !props.service_is_a(&id, ServiceType::Scheduler) {
        return init;
    }

    let clients = props.service_clients(&id, ServiceType::Scheduler);

    assert!(props.service_is_a(&id, ServiceType::Scheduler));
    if let Some(cs) = clients {
        for c in cs.iter() {
            init.push(
                // key: component id and value: style of initialization
                ArgsKV::new_key(
                    c.to_string(),
                    if props.service_is_a(c, ServiceType::Scheduler) {
                        "sched".to_string()
                    } else {
                        "init".to_string()
                    },
                ),
            );
        }
    }

    // The pushes reversed the order
    init.reverse();
    init
}

fn sched_config(s: &SystemState, id: &ComponentId, cfg: &mut CompConfigState) {
    cfg.args.push(ArgsKV::new_arr(
        "execute".to_string(),
        sched_config_clients(&s, &id),
    ));
}

fn cap2kvarg(capid: u32, cap: &CapRes) -> ArgsKV {
    let (name, target) = cap_info(&cap);
    let capinfo = vec![
        ArgsKV::new_key("type".to_string(), name),
        ArgsKV::new_key("target".to_string(), target.to_string()),
    ];
    ArgsKV::new_arr(capid.to_string(), capinfo)
}

fn capmgr_config(s: &SystemState, id: &ComponentId, cfg: &mut CompConfigState) {
    let props: &PropertiesPass = s.get_properties();
    if !props.service_is_a(&id, ServiceType::CapMgr) {
        return;
    }

    let parent = props.service_dependency(&id, ServiceType::Scheduler);
    let mut clients = props
        .service_clients(&id, ServiceType::Scheduler)
        .map(|cs| cs.clone()) // get rid of the reference
        .unwrap_or_else(|| Vec::new());
    let mut sched_args = Vec::new();
    let mut ct_args = Vec::new();
    let mut init_args = Vec::new();
    let mut names_args = Vec::new();

    // aggregate records for scheduler and capmgr dependencies
    clients.append(
        &mut props
            .service_clients(&id, ServiceType::CapMgr)
            .map(|cs| cs.clone())
            .unwrap_or_else(|| Vec::new()),
    );
    clients.sort();
    clients.dedup();

    assert!(props.service_is_a(&id, ServiceType::CapMgr));
    for c in &clients {
        // sanity
        assert!(*c != *id);
        // don't support nested capmgrs yet
        assert!(!props.service_is_a(&c, ServiceType::CapMgr));

        // capability table entries for the client
        cfg.ct.add(CapRes::CapTbl(*c));
        cfg.ct.add(CapRes::PgTbl(*c));
        cfg.ct.add(CapRes::Comp(*c));

        // scheduler hierarchy
        if (props.service_is_a(&c, ServiceType::Scheduler)) {
            let p = props.service_dependency(&c, ServiceType::Scheduler);

            // at the least, the capmgr should be the parent
            assert!(p.is_some());
            sched_args.push(ArgsKV::new_key(c.to_string(), p.unwrap().to_string()));
        }

        // Initialization information for all components shipped to
        // the capmgr This effectively grants permission for the
        // scheduler to create execution in a client.
        let mut init_clients = sched_config_serv_client(&s, &c);
        if !init_clients.is_empty() {
            init_args.append(&mut init_clients);
        }

        // client names
        let spec_comp = component(&s, &c);
        let name = format!(
            "{}.{}.{}",
            spec_comp.source, spec_comp.name.scope_name, spec_comp.name.var_name
        );
        names_args.push(ArgsKV::new_key(c.to_string(), name));
    }
    cfg.args.push(ArgsKV::new_arr(
        "scheduler_hierarchy".to_string(),
        sched_args,
    ));
    cfg.args
        .push(ArgsKV::new_arr("init_hierarchy".to_string(), init_args));

    for (capid, cap) in cfg.ct.get_captbl() {
        ct_args.push(cap2kvarg(*capid, &cap));
    }
    cfg.args
        .push(ArgsKV::new_arr("captbl".to_string(), ct_args));
    cfg.args
        .push(ArgsKV::new_arr("names".to_string(), names_args));
}

fn constructor_config(s: &SystemState, id: &ComponentId, cfg: &mut CompConfigState) {
    let props: &PropertiesPass = s.get_properties();
    if !props.service_is_a(&id, ServiceType::Constructor) {
        return;
    }

    let clients = props.service_clients(&id, ServiceType::Constructor);
    let mut captbls = BTreeMap::new();

    assert!(props.service_is_a(&id, ServiceType::Constructor));
    // for now, assume only a single constructor
    assert!(props
        .service_dependency(&id, ServiceType::Constructor)
        .is_none());
    if let Some(cs) = clients {
        for c in cs {
            // only have captbl for capmgrs
            if !props.service_is_a(&c, ServiceType::CapMgr) {
                continue;
            }

            // Assuming that the values returned by this are the
            // same as those created while processing the capmgr
            let mut chld_cfg = CompConfigState::new();
            capmgr_config(&s, &c, &mut chld_cfg);
            captbls.insert(*c, chld_cfg);
        }
    }

    // OK, then we should construct the arguments
    let mut args = Vec::new();
    for (id, cfg) in &captbls {
        let mut comp_args = Vec::new();
        let ct = cfg.ct.get_captbl();

        for (capid, cap) in ct {
            comp_args.push(cap2kvarg(*capid, &cap));
        }

        args.push(ArgsKV::new_arr(id.to_string(), comp_args));
    }

    cfg.args
        .push(ArgsKV::new_arr("captbl_delegations".to_string(), args));
    // FIXME: move some of the build.rs logic for constructor creation here.
}

fn comp_config_finalize(s: &SystemState, id: &ComponentId, cfg: CompConfigState) -> Vec<ArgsKV> {
    let frontier = cfg.ct.get_frontier();
    let mut cfg_mut = cfg;

    // TODO: Note that this is the end *before* all of the sinv allocations
    cfg_mut.args.push(ArgsKV::new_key(
        "captbl_end".to_string(),
        frontier.to_string(),
    ));
    // this clone is the cost of the type making it clear this is the
    // "sink" for cfg
    cfg_mut.args.clone()
}

pub struct ResAssignPass {
    resources: HashMap<ComponentId, Vec<ArgsKV>>,
}

impl Transition for ResAssignPass {
    fn transition(s: &SystemState, b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let prop = s.get_properties();
        let mut res = HashMap::new();

        for (k, v) in s.get_named().ids().iter() {
            let mut cfg = CompConfigState::new();

            // capmgr configuration must be first, and before
            // constructor configuration, as constructor has to be
            // able to replicate each capmgr's captbl layout
            capmgr_config(&s, &k, &mut cfg);
            constructor_config(&s, &k, &mut cfg);
            sched_config(&s, &k, &mut cfg);
            comp_config(&s, &k, &mut cfg);
            res.insert(k.clone(), comp_config_finalize(&s, &k, cfg));
        }

        Ok(Box::new(ResAssignPass { resources: res }))
    }
}

impl ResPass for ResAssignPass {
    fn args(&self, id: &ComponentId) -> &Vec<ArgsKV> {
        &self.resources.get(&id).unwrap()
    }
}
