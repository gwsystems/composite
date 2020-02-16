use initargs::ArgsKV;
use passes::{
    deps, BuildState, ComponentId, Interface, PropertiesPass, ResPass, ServiceType, SystemState,
    Transition,
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
enum CapTarget {
    Ourselves,
    Client(ComponentId),
    Indirect(ComponentId, ComponentId), // component with access to a cap for another component
}

#[derive(Debug)]
enum CapRes {
    CapTbl(CapTarget),
    PgTbl(CapTarget),
    Comp(CapTarget),
    Thd(CapTarget),
    SInv, // only used to lookup its size
    TCap(CapTarget),
    Rcv(CapTarget), // ...
}

// The equivalent of the C __captbl_cap2sz(c)
fn cap_sz(cap: &CapRes) -> u32 {
    match cap {
        CapRes::Thd(_) | CapRes::TCap(_) => 1,
        CapRes::CapTbl(_) | CapRes::PgTbl(_) => 2,
        CapRes::Rcv(_) | CapRes::SInv | CapRes::Comp(_) => 4,
    }
}

fn cap_targets(t: &CapTarget) -> (Option<ComponentId>, Option<ComponentId>) {
    match t {
        CapTarget::Ourselves => (None, None),
        CapTarget::Client(id) => (Some(*id), None),
        CapTarget::Indirect(chld, grandchld) => (Some(*chld), Some(*grandchld)),
    }
}

// returns the size of the capability, the type (in captbl, pgtbl,
// comp, for now), the target, and the indirect target.
fn cap_info(c: &CapRes) -> (u32, String, Option<ComponentId>, Option<ComponentId>) {
    let sz = cap_sz(&c);
    let (n, (c, gc)) = match c {
        CapRes::CapTbl(t) => ("captbl".to_string(), cap_targets(t)),
        CapRes::PgTbl(t) => ("pgtbl".to_string(), cap_targets(t)),
        CapRes::Comp(t) => ("comp".to_string(), cap_targets(t)),
        _ => unimplemented!("Capabilities other than pgtbl, captbl, and component not supported"),
    };

    (sz, n, c, gc)
}

pub struct ResAssignPass {
    resources: HashMap<ComponentId, Vec<ArgsKV>>,
}

fn sched_config(s: &SystemState, id: &ComponentId, mut args: Vec<ArgsKV>) -> Vec<ArgsKV> {
    let props: &PropertiesPass = s.get_properties();
    let clients = props.service_clients(&id, ServiceType::Scheduler);
    let parent = props.service_dependency(&id, ServiceType::Scheduler);
    let mut init = Vec::new();

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

    args.push(ArgsKV::new_arr("execute".to_string(), init));
    args
}

fn capmgr_config(
    s: &SystemState,
    id: &ComponentId,
    mut ct: Vec<CapRes>,
    mut ias: Vec<ArgsKV>,
) -> (Vec<CapRes>, Vec<ArgsKV>) {
    let props: &PropertiesPass = s.get_properties();
    let mut args = Vec::new();
    let parent = props.service_dependency(&id, ServiceType::Scheduler);
    let mut clients = props
        .service_clients(&id, ServiceType::Scheduler)
        .map(|cs| cs.clone()) // get rid of the reference
        .unwrap_or_else(|| Vec::new());

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
        ct.push(CapRes::CapTbl(CapTarget::Client(*c)));
        ct.push(CapRes::PgTbl(CapTarget::Client(*c)));
        ct.push(CapRes::Comp(CapTarget::Client(*c)));

        // scheduler hierarchy
        if (props.service_is_a(&c, ServiceType::Scheduler)) {
            let p = props.service_dependency(&c, ServiceType::Scheduler);

            // at the least, the capmgr should be the parent
            assert!(p.is_some());
            args.push(ArgsKV::new_key(c.to_string(), p.unwrap().to_string()));
        }
    }

    ias.push(ArgsKV::new_arr("scheduler_hierarchy".to_string(), args));
    (ct, ias)
}

fn constructor_config(
    s: &SystemState,
    id: &ComponentId,
    mut ct: Vec<CapRes>,
    mut args: Vec<ArgsKV>,
) -> (Vec<CapRes>, Vec<ArgsKV>) {
    let props: &PropertiesPass = s.get_properties();
    let clients = props.service_clients(&id, ServiceType::Constructor);
    let parent = props.service_dependency(&id, ServiceType::Constructor);

    assert!(props.service_is_a(&id, ServiceType::Constructor));
    assert!(parent.is_none());
    if let Some(cs) = clients {
        for c in cs.iter() {
            // assert!();

            if props.service_is_a(&c, ServiceType::CapMgr) {
                let mut cm_ct = Vec::new();
                // Assuming that the values returned by this are the
                // same as those created while processing the capmgr
                let (cm_ct, _) = capmgr_config(&s, &c, cm_ct, Vec::new());

                for cap in &cm_ct {
                    let cr = match cap {
                        CapRes::CapTbl(CapTarget::Client(t)) => {
                            Some(CapRes::CapTbl(CapTarget::Indirect(*c, *t)))
                        }
                        CapRes::PgTbl(CapTarget::Client(t)) => {
                            Some(CapRes::PgTbl(CapTarget::Indirect(*c, *t)))
                        }
                        CapRes::Comp(CapTarget::Client(t)) => {
                            Some(CapRes::Comp(CapTarget::Indirect(*c, *t)))
                        }
                        _ => None,
                    };
                    if let Some(res) = cr {
                        ct.push(res);
                    }
                }
            }
        }
    }

    // FIXME: move some of the build.rs logic for constructor creation here.

    (ct, args)
}

fn captbl_serialize(ct: Vec<CapRes>) -> Vec<ArgsKV> {
    let mut args = Vec::new();
    let mut all = Vec::new();
    let mut capid = 4; // the first 4 capabilities are 1 slot and ret
    let mut prev_capsz = 1;

    for r in &ct {
        let (sz, name, target, indirect) = cap_info(&r);

        // all capabilities with in each block of 4 capids must have the same id
        if (capid % 4 != 0) && (prev_capsz != sz) {
            capid = capid + (4 - capid % 4); //  round up to power of 4
        }

        let mut capinfo = vec![ArgsKV::new_key("type".to_string(), name)];
        if let Some(t) = target {
            capinfo.push(ArgsKV::new_key("target".to_string(), t.to_string()));
        }
        if let Some(it) = indirect {
            capinfo.push(ArgsKV::new_key("indirect".to_string(), it.to_string()));
        }

        args.push(ArgsKV::new_arr(capid.to_string(), capinfo));

        prev_capsz = sz;
        capid = capid + sz;
    }

    if args.len() > 0 {
        all.push(ArgsKV::new_arr("captbl".to_string(), args));
    }
    if (capid % 4 != 0) {
        capid = capid + (4 - capid % 4); //  round up to power of 4
    }
    all.push(ArgsKV::new_key("captbl_end".to_string(), capid.to_string()));

    all
}

impl Transition for ResAssignPass {
    fn transition(s: &SystemState, b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let prop = s.get_properties();
        let mut res = HashMap::new();

        for (k, v) in s.get_named().ids().iter() {
            let mut ct = Vec::new();
            let mut args = Vec::new();
            let mut ctargs = Vec::new();

            if prop.service_is_a(k, ServiceType::Scheduler) {
                args = sched_config(&s, &k, args);
            }
            if prop.service_is_a(k, ServiceType::CapMgr) {
                let ret = capmgr_config(&s, &k, ct, args);
                ct = ret.0;
                args = ret.1;
            }
            if prop.service_is_a(k, ServiceType::Constructor) {
                let ret = constructor_config(&s, &k, ct, args);
                ct = ret.0;
                args = ret.1;
            }

            ctargs = captbl_serialize(ct);
            args.append(&mut ctargs);

            res.insert(k.clone(), args);
        }

        Ok(Box::new(ResAssignPass { resources: res }))
    }
}

impl ResPass for ResAssignPass {
    fn args(&self, id: &ComponentId) -> &Vec<ArgsKV> {
        &self.resources.get(&id).unwrap()
    }
}
