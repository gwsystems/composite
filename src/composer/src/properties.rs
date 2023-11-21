use passes::{
    component, deps, BuildState, ComponentId, Interface, PropertiesPass, ServiceClients,
    ServiceProvider, ServiceType, SystemState, Transition,
};
use std::collections::HashMap;

pub struct CompProperties {
    comps: HashMap<ComponentId, (Vec<ServiceClients>, Vec<ServiceProvider>)>,
}

// Return if we depend on (yet implement as a library) the given
// interface, Create the set of components we provide an interface
// for, and the parent we depend on for that interface.
fn interface_dependencies(
    s: &SystemState,
    id: &ComponentId,
    if_name: Interface,
) -> (bool, Vec<ComponentId>, Option<ComponentId>) {
    let ids = s.get_named().ids();
    let n = ids.get(&id).unwrap();

    // TODO: This is insufficient. We also need to see if
    // corresponding libraries are depended on here which mean we're
    // providing the service in question.
    let us = deps(&s, &id)
        .iter()
        .find(|ref d| d.interface == if_name && d.variant == "kernel")
        .is_some();

    let clients: Vec<ComponentId> = ids
        .iter()
        .filter_map(|(ref cli_id, ref _cli_name)| {
            // is the client dependent on us for initialization?
            if deps(&s, &cli_id)
                .iter()
                .find(|&d| d.server == *n && d.interface == if_name && d.variant != "kernel")
                .is_some()
            {
                Some(**cli_id)
            } else {
                None
            }
        })
        .collect();

    let parent = deps(&s, &id)
        .iter()
        .find(|ref d| d.interface == if_name && d.variant != "kernel")
        .map(|ref d| s.get_named().rmap().get(&d.server).unwrap().clone());

    (us, clients, parent)
}

impl Transition for CompProperties {
    fn transition(s: &SystemState, _b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let mut properties = HashMap::new();

        for (id, _) in s.get_named().ids().iter() {
            let mut props = Vec::new();
            let mut parents = Vec::new();

            // Scheduler properties
            let (is_an_init, prop, parent) = interface_dependencies(&s, &id, "init".to_string());
            if prop.len() > 0 || is_an_init {
                props.push(ServiceClients::Scheduler(prop));
            }
            if let Some(p) = parent {
                parents.push(ServiceProvider::Scheduler(p));
            }

            // Capability manager properties
            let (is_a_capmgr, mut prop, parent) = interface_dependencies(&s, &id, "capmgr".to_string());
            // Capability manager (with only thread creation) properties
            let (is_a_capmgrthd, mut prop2, parent2) = interface_dependencies(&s, &id, "capmgr_create".to_string());
            if prop.len() + prop2.len() > 0 || is_a_capmgr || is_a_capmgrthd {
                prop.append(&mut prop2);
                props.push(ServiceClients::CapMgr(prop));
            }
            if let Some(p) = parent {
                if let Some(p2) = parent2 {
                    if p != p2 {
                        return Err(format!("Error: Component {} depends on both capmgr and capmgr_create, but from different components ({} and {}).", id, p, p2));
                    }
                }
                parents.push(ServiceProvider::CapMgr(p));
            }

            // Constructor properties
            let (prop, parent) =
                s.get_named()
                    .ids()
                    .iter()
                    .fold((Vec::new(), None), |(mut p, par), (id2, _)| {
                        let cons = &component(s, id2).constructor;
                        if cons.var_name == "kernel" {
                            return (p, par);
                        }
                        let cons_id = s.get_named().rmap().get(cons).unwrap();
                        if id == id2 {
                            // this is us; set our parent
                            (p, Some(cons_id.clone()))
                        } else if cons_id == id {
                            // this is a client, add them
                            p.push(id2.clone());
                            (p, par)
                        } else {
                            // irrelevant component, pass on through
                            (p, par)
                        }
                    });
            // If we have constructor clients, or are the only
            // component, we're a constructor!
            if prop.len() > 0 || s.get_named().ids().len() == 1 {
                props.push(ServiceClients::Constructor(prop));
            }
            if let Some(p) = parent {
                parents.push(ServiceProvider::Constructor(p));
            }

            properties.insert(id.clone(), (props, parents));
        }

        Ok(Box::new(CompProperties { comps: properties }))
    }
}

impl PropertiesPass for CompProperties {
    // Subtle return value. None means the component is *not* of the
    // requested type. A vector with 0 <= n component is of the
    // requested type *and* has the vector's contents as clients
    fn service_clients(&self, id: &ComponentId, t: ServiceType) -> Option<&Vec<ComponentId>> {
        let c_opt = self.comps.get(id);
        if let Some(c) = c_opt {
            c.0.iter().find_map(|r| match t {
                ServiceType::Scheduler => match r {
                    ServiceClients::Scheduler(c) => Some(c),
                    _ => None,
                },
                ServiceType::CapMgr => match r {
                    ServiceClients::CapMgr(c) => Some(c),
                    _ => None,
                },
                ServiceType::Constructor => match r {
                    ServiceClients::Constructor(c) => Some(c),
                    _ => None,
                },
            })
        } else {
            None
        }
    }

    // Is the compoent a service of the type?
    fn service_is_a(&self, id: &ComponentId, t: ServiceType) -> bool {
        self.service_clients(&id, t).is_some()
    }

    fn service_dependency(&self, id: &ComponentId, t: ServiceType) -> Option<ComponentId> {
        let c_opt = self.comps.get(id);
        if let Some(c) = c_opt {
            c.1.iter().find_map(|r| match t {
                ServiceType::Scheduler => match r {
                    ServiceProvider::Scheduler(c) => Some(c.clone()),
                    _ => None,
                },
                ServiceType::CapMgr => match r {
                    ServiceProvider::CapMgr(c) => Some(c.clone()),
                    _ => None,
                },
                ServiceType::Constructor => match r {
                    ServiceProvider::Constructor(c) => Some(c.clone()),
                    _ => None,
                },
            })
        } else {
            None
        }
    }
}
