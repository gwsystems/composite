use ascent::{ascent_run, lattice::Dual};
use passes::{AnalysisPass, VirtResPass, BuildState, ComponentId, Interface, SystemState, Transition, component, ComponentName};
use std::collections::HashMap;
use std::collections::HashSet;

// A very simplistic criticality-level specification.
type CriticalityLvl = usize;
#[allow(dead_code)]
const HC: CriticalityLvl = 1;
#[allow(dead_code)]
const LC: CriticalityLvl = 0;
#[allow(dead_code)]
const NUM_CRIT: usize = 2;

// Component properties that are useful in determining how they impact
// other components.
#[allow(dead_code)]
#[derive(Clone, PartialEq, Eq, Hash, Debug)]
enum CompProperties {
    Scheduler,
    CapMgr,
    MemMgr,
    Constructor,
    MutExcl,
    DynMem,
    CanBlock,
}

// The set of warnings we're deriving from the system structure. These
// warnings are associated with a specific component.
#[derive(Clone, PartialEq, Eq, Hash)]
pub enum Warning {
    // A shared component service services multiple criticalities
    SharedServiceMultCrit(CriticalityLvl, CriticalityLvl),
    // A LC component (first argument of tuple) can block a HC
    // component (the component parameterized with this warning),
    // through a virtual resource
    BlockingMultCrit(ComponentId, VirtResource, VirtResourceId),
    // Is there a potential DoS on a shared pool of resources? Can
    // *both* a HC and a LC component dynamically allocate
    PotentialDos(ComponentId, VirtResource),
    // The server component uses locks and can cause interference
    // on the listed component .
    PotentialInterference(ComponentId),
    // The server component uses locks that can cause interference
    // between the send and third components are different
    // criticalities.
    PotentialInterferenceMultCrit(ComponentId, ComponentId),
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
#[allow(dead_code)]
#[derive(Clone, PartialEq, Eq, Hash, Debug)]
enum VirtResAccess {
    Read,        // read/access resource
    Modify,      // modify/write to resource
    DynAlloc,    // dynamically allocate resources
    Block,       // can a component block on the resource?
    Unspecified, // The interface is unlabeled (e.g. no virt resources)
}

#[allow(dead_code)]
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
    virt_res_service: Vec<(VirtResource, ComponentId)>,
    // The access of clients to specific virtual resources
    virt_res_access: Vec<(VirtResource, VirtResourceId, ComponentId, VirtResAccess)>,
}

#[allow(dead_code)]
pub struct Analysis {
    // Simple transitive closure over the `dependencies` relation for
    // (client, server)
    depends_on: Vec<(ComponentId, ComponentId)>,
    // All of the properties for each component
    comp_properties: Vec<(ComponentId, CompProperties)>,
    // The range of criticalities (min, max) for a specific component
    comp_crit_range: Vec<(ComponentId, usize, usize)>,
    // The first component can block the second.
    comp_can_block: Vec<(ComponentId, ())>,
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

    // The warnings we derive from all of the above data.
    warnings: Vec<(ComponentId, Warning)>,

    // The public warnings hashmap to get warnings for a specific
    // component.
    pub_warnings: HashMap<ComponentId, Vec<Warning>>,
}

fn analysis_output(i: AnalysisInput) -> Analysis {
    let srv_prop_map: Vec<(String, CompProperties)> = SERVER_PROPERTIES
        .iter()
        .map(|(i, p)| (i.to_string(), p.clone()))
        .collect();
    let cli_prop_map: Vec<(String, CompProperties)> = CLIENT_PROPERTIES
        .iter()
        .map(|(i, p)| (i.to_string(), p.clone()))
        .collect();

    // Destructure, so that we can individually move the vectors
    // into the program.
    let AnalysisInput {
        criticalities,
        dependencies,
        virt_res_service,
        virt_res_access,
        ..
    } = i;

    let p = ascent_run! {

        // inputs
        relation criticalities(ComponentId, CriticalityLvl) = criticalities;
        relation dependencies(ComponentId, ComponentId, Interface, VirtResAccess) = dependencies;
        relation srv_prop_map(String, CompProperties) = srv_prop_map;
        relation cli_prop_map(String, CompProperties) = cli_prop_map;
        relation virt_res_service(VirtResource, ComponentId) = virt_res_service;
        relation virt_res_access(VirtResource, VirtResourceId, ComponentId, VirtResAccess) = virt_res_access;

        // transient relations
        relation component(ComponentId);
        relation criticality_exposure(ComponentId, CriticalityLvl);
        lattice comp_crit_hi(ComponentId, CriticalityLvl);
        lattice comp_crit_lo(ComponentId, Dual<CriticalityLvl>);

        // outputs
        relation depends_on(ComponentId, ComponentId);
        relation comp_properties(ComponentId, CompProperties);
        relation comp_crit_range(ComponentId, usize, usize);
        relation comp_can_block(ComponentId, ());
        relation comp_dos(ComponentId, ComponentId, VirtResource);
        relation comp_shared_lock(ComponentId, ComponentId);
        relation comp_nested_lock(ComponentId, ComponentId);
        relation show_warnings(ComponentId);

        // The main output
        relation warnings(ComponentId, Warning);

        // Normal transitive closure
        depends_on(c, s) <-- dependencies(c, s, _, _);
        depends_on(c, ss) <-- depends_on(c, s), dependencies(s, ss, _, _);

        //hard-coded assign constructor properties to booter,logic is realted to tot_order.rs
        comp_properties(1, CompProperties::Constructor);       
        // server properties
        comp_properties(s, p) <-- dependencies(_, s, i, _), srv_prop_map(i, p);
        // client properties
        comp_properties(c, p) <-- dependencies(c, _, i, _), cli_prop_map(i, p);

        // Compute the criticality exposure for each component
        criticality_exposure(c, crit) <-- criticalities(c, crit);
        criticality_exposure(s, crit) <-- depends_on(c, s), criticalities(c, crit);
        comp_crit_hi(c, crit) <-- criticality_exposure(c, crit);
        comp_crit_lo(c, Dual(*crit)) <-- criticality_exposure(c, crit);
        // Potential bug: if the lattice isn't completely computed
        // before processing this relation, then we might have
        // multiple `comp_crit_range(c, _, _)` entries.
        comp_crit_range(c, hi, lo) <-- comp_crit_hi(c, hi), comp_crit_lo(c, ?Dual(lo));

        component(c) <-- dependencies(c, _, _, _);
        component(s) <-- dependencies(_, s, _, _);

        show_warnings(c) <-- component(c), 
            !comp_properties(c, CompProperties::Scheduler), !comp_properties(c, CompProperties::CapMgr), 
            !comp_properties(c, CompProperties::Constructor), !comp_properties(c, CompProperties::MemMgr);

        warnings(c, Warning::SharedServiceMultCrit(*hi, *lo)) <-- show_warnings(c), comp_crit_range(c, hi, lo), if lo < hi;

        // TODO: take into account blocking on shared virtual resources
        comp_can_block(c, ()) <-- comp_properties(c, CompProperties::CanBlock);

        // If components of different criticalities share a virtual
        // resource, and the higher criticality one can block, we have
        // a progress issue for the higher criticality one.
        warnings(hc, Warning::BlockingMultCrit(*lc, vr.clone(), *vrid)) <-- show_warnings(hc),
            comp_crit_hi(hc, hi_crit), comp_crit_lo(lc, ?Dual(lo_crit)), if lo_crit < hi_crit,
            virt_res_access(vr, vrid, hc, VirtResAccess::Block), virt_res_access(vr, vrid, lc, _);

        // If both HC and LC components can dynamically allocate
        // virtual resources in a service, the LC might exhaust
        // resources, DoSing the HC task.
        warnings(hc, Warning::PotentialDos(*sc, vr.clone())) <-- 
            virt_res_access(vr, _, hc, VirtResAccess::DynAlloc), virt_res_access(vr, _, lc, VirtResAccess::DynAlloc),
            virt_res_service(vr, sc), comp_crit_hi(hc, hi), comp_crit_lo(lc, ?Dual(lo)), if lo < hi;

        comp_shared_lock(s, c) <-- comp_properties(s, CompProperties::MutExcl), depends_on(c, s);
        comp_nested_lock(s, c) <-- comp_properties(s, CompProperties::MutExcl), comp_properties(c, CompProperties::MutExcl), depends_on(c, s);

        // Is a component's threads execute potentially impacted by
        // mutual exclusion in a server component?
        warnings(s, Warning::PotentialInterference(*c)) <-- show_warnings(s), comp_shared_lock(s, c);
        // Do both LC and HC components share a component that uses
        // mutual exclusion?
        warnings(s, Warning::PotentialInterferenceMultCrit(*hc, *lc)) <-- show_warnings(s),
            comp_shared_lock(s, hc), comp_shared_lock(s, lc),
            comp_crit_hi(hc, hi), comp_crit_lo(lc, lo), if hi > lo;
    };
    
    println!("show_warnings: {:?}", p.show_warnings.iter().collect::<Vec<_>>());


    let lock_data: Vec<_> = p.comp_shared_lock.iter().collect();
    println!("shared lock: {:?}", lock_data);

    let vr_data: Vec<_> = p.virt_res_access.iter().collect();
    println!("virtual resource list: {:?}", vr_data);

    let properties_data: Vec<_> = p.comp_properties.iter().collect();
    println!("properties lock: {:?}", properties_data);

    let dependencies_data: Vec<_> = p.dependencies.iter().collect();
    println!("dependencies: {:?}", dependencies_data);

    let comp_crit_hi_data: Vec<_> = p.comp_crit_hi.iter().collect();
    let comp_crit_lo_data: Vec<_> = p.comp_crit_lo.iter().collect();
    let comp_crit_range_data: Vec<_> = p.comp_crit_range.iter().collect();

    println!("comp_crit_hi: {:?}", comp_crit_hi_data);
    println!("comp_crit_lo: {:?}", comp_crit_lo_data);
    println!("comp_crit_range: {:?}", comp_crit_range_data);

    let depends_on_data: Vec<_> = p.depends_on.iter().collect();
    println!("depends_on: {:?}", depends_on_data);

    let mut ws: HashMap<ComponentId, Vec<Warning>> = HashMap::new();
    for (c, w) in &p.warnings {
        if let Some(e) = ws.get_mut(&c) {
            e.push(w.clone());
        } else {
            ws.insert(c.clone(), vec![w.clone()]);
        }
    }

    Analysis {
        depends_on: p.depends_on,
        comp_properties: p.comp_properties,
        comp_crit_range: p.comp_crit_range,
        comp_can_block: p.comp_can_block,
        comp_dos: p.comp_dos,
        comp_shared_lock: p.comp_shared_lock,
        comp_nested_lock: p.comp_nested_lock,
        warnings: p.warnings,
        pub_warnings: ws,
    }
}

impl Analysis {
    fn expand_access_options_in_deps(
        s: &SystemState,
        dependencies: Vec<(ComponentId, ComponentId, Interface, VirtResAccess)>
    ) -> Vec<(ComponentId, ComponentId, Interface, VirtResAccess)> {
        
        let mut extended_dependencies: Vec<(ComponentId, ComponentId, Interface, VirtResAccess)> = Vec::new();
    
        let full_access: HashSet<VirtResAccess> = [
            VirtResAccess::Read,
            VirtResAccess::Modify,
            VirtResAccess::DynAlloc,
            VirtResAccess::Block,
        ].iter().cloned().collect();


        for dep in dependencies {
            let client_id = dep.0;
            let server_id = dep.1;
            let interface = &dep.2;
    
            let c = component(&s, &client_id);
            
            let interface_toml = s.get_spec().interface_funcs().get(interface);
    
            let mut vr_access: HashSet<VirtResAccess> = HashSet::new();

            if let Some(interface_toml) = interface_toml {                            
                for vr in &c.virt_res {
                    if interface_toml.virt_resources == vr.vr_type {
                        for inst in &vr.instances {
                            for (_vr_inst_name, vr_inst_config) in inst {
                                for access in &vr_inst_config.access {
                                    match access.as_str() {
                                        "read" => { vr_access.insert(VirtResAccess::Read); },
                                        "write" => { vr_access.insert(VirtResAccess::Modify); },
                                        "dynamic_alloc" => { vr_access.insert(VirtResAccess::DynAlloc); },
                                        "blocking" => { vr_access.insert(VirtResAccess::Block); },
                                        _ => {},
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if vr_access.is_empty() {
                vr_access = full_access.clone();
            }
            
            for access in &vr_access {
                extended_dependencies.push((
                    client_id,
                    server_id,
                    interface.clone(),
                    access.clone(),
                ));
            }
        }
    
        extended_dependencies
    }
    
    fn build(s: &SystemState) -> Self {
        let cs = s.get_spec();
        //let vr_orig = s.get_spec().virtual_resources();
        //s.get_spec().interface_funcs()
        let ids = s.get_named();
        let vr_pass: &dyn VirtResPass = s.get_virt_res();

        let compid = |c| ids.rmap().get(c).unwrap();
        let components: Vec<ComponentId> = cs.names().iter().map(|c| *compid(c)).collect();
        // TODO: get the criticalities from the spec
        let criticalities: Vec<(ComponentId, CriticalityLvl)> =components.iter().map(|c| {
            let criticality_level = component(&s, &c).criticality_level.clone().unwrap_or(LC);
            (*c, criticality_level)
        }).collect();
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
 
        let full_dependencies = Self::expand_access_options_in_deps(s, dependencies);

        let mut virt_res_service: Vec<(VirtResource, ComponentId)> = Vec::new();
        let vr_orig = s.get_spec().virtual_resources();
        for (_key, vr) in vr_orig {
            let component_name = ComponentName::new(&vr.server, &String::from("global"));
            virt_res_service.push((vr.vr_type.clone(),*ids.rmap().get(&component_name).unwrap()));
        }  
                
        let mut virt_res_access: Vec<(VirtResource, VirtResourceId, ComponentId, VirtResAccess)> = Vec::new();
        for id in &components {
            let comp_spec = component(s,id);
            for vr in &comp_spec.virt_res {
                for inst in &vr.instances {
                    for (vr_inst_name, vr_inst_config) in inst {
                        for access in &vr_inst_config.access {
                            //TBD: santiy check on the access option writting. 
                            let access_type = match access.as_str() {
                                "read" => VirtResAccess::Read,
                                "write" => VirtResAccess::Modify,
                                "dynamic_alloc" => VirtResAccess::DynAlloc,
                                "blocking" => VirtResAccess::Block,
                                _ => VirtResAccess::Unspecified,
                            };
                            virt_res_access.push((
                                vr.vr_type.clone(),                     
                                *vr_pass.rmap().get(&vr_inst_name.clone()).unwrap(), 
                                *id,                                    
                                access_type                             
                            ));
                        }
                    }
                }
            }
        }

        for (virt_resource, component_id) in &virt_res_service {
            println!("VirtResource: {}, ComponentId: {}", virt_resource, component_id);
        }

        for (virt_resource, virt_resource_id, component_id, access_type) in &virt_res_access {
            println!(
                "VirtResource: {}, VirtResourceId: {}, ComponentId: {}, AccessType: {:?}",
                virt_resource, virt_resource_id, component_id, access_type
            );
        }

        // Calculate the analysis outputs
        analysis_output(AnalysisInput {
            components,
            dependencies: full_dependencies,
            criticalities,
            // TODO
            virt_res_service,
            virt_res_access,
        })
    }
}

impl AnalysisPass for Analysis {
    fn warnings(&self) -> &HashMap<ComponentId, Vec<Warning>> {
        &self.pub_warnings
    }

    fn warning_str(&self, id: ComponentId, s: &SystemState) -> String {
        let ids = s.get_named();
        let name = |id| ids.ids().get(&id).unwrap();
        let compname = name(id);
        let ws_map = self.warnings();

        if let Some(ws) = ws_map.get(&id) {
            ws.iter().fold(format!("Component {compname} warnings:\n"), |s, w| {
                match w {
                Warning::SharedServiceMultCrit(hi, lo) =>
                    format!("{s}\tService shared between components of criticalities spanning from {lo} to {hi}\n"),
                Warning::BlockingMultCrit(id, vr, vrid) =>
                    format!("{s}\tThis higher criticality component can be blocked on virtual resource {vr} (id {vrid}) that is shared with component {}\n", name(*id)),
                Warning::PotentialDos(id, vr) =>
                    format!("{s}\tThis service exports dynamic allocation for virtual resource {vr} to various criticality components, opening the high-criticality component {} up to a DoS attack\n", name(*id)),
                Warning::PotentialInterference(id) =>
                    format!("{s}\tThis component uses locks, thus can impose low-priority interference on higher priority tasks in component {}\n", name(*id)),
                Warning::PotentialInterferenceMultCrit(id1, id2) =>
                    format!("{s}\tThe component uses locks and might cause cross-criticality inteference in components {} and {}\n", name(*id1), name(*id2)),
                }
            })
        } else {
            format!("Component {compname} has no warnings.\n")
        }
    }
}

impl Transition for Analysis {
    fn transition(s: &SystemState, _b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let a = Analysis::build(s);

        Ok(Box::new(a))
    }
}