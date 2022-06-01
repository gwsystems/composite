use passes::{
    AddrSpace, AddrSpaces, BuildState, ComponentId, ComponentName, OrderedSpecPass, SystemState,
    Transition,
};
use std::collections::{BTreeMap, HashSet};

pub struct CompTotOrd {
    ids: BTreeMap<ComponentId, ComponentName>,
    rmap: BTreeMap<ComponentName, ComponentId>,
    addrspc_comps: BTreeMap<usize, AddrSpace>,
    addrspc_exclusive: Vec<ComponentName>,
}

impl OrderedSpecPass for CompTotOrd {
    fn ids(&self) -> &BTreeMap<ComponentId, ComponentName> {
        &self.ids
    }

    fn rmap(&self) -> &BTreeMap<ComponentName, ComponentId> {
        &self.rmap
    }

    fn addrspc_components_shared(&self) -> &BTreeMap<usize, AddrSpace> {
        &self.addrspc_comps
    }

    fn addrspc_components_exclusive(&self) -> &Vec<ComponentName> {
        &self.addrspc_exclusive
    }
}

fn addrspc_dfs_via_children(
    offset: &mut usize,
    agg: &mut BTreeMap<usize, AddrSpace>,
    curr: &AddrSpace,
    addrspaces: &AddrSpaces,
) {
    agg.insert(*offset, curr.clone());
    *offset = *offset + 1;
    for child_name in &curr.children {
        // Unwrap should be OK as we've already validated the
        // component names.
        let child = addrspaces.get(child_name).unwrap();
        addrspc_dfs_via_children(offset, agg, child, addrspaces);
    }
}

impl Transition for CompTotOrd {
    fn transition(s: &SystemState, _b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let spec = s.get_spec();

        // Find a total order of components based on the dependency
        // relation...
        let mut tot_ord = Vec::new();
        // which components are still remaining without a placement
        // in the total order?
        let mut remaining: BTreeMap<ComponentName, Vec<ComponentName>> = BTreeMap::new();
        for n in spec.names().iter() {
            let cons = &spec.component_named(&n).constructor;
            let mut ds: Vec<ComponentName> = spec
                .deps_named(n)
                .iter()
                .map(|d| d.server.clone())
                .collect();
            if cons.var_name != "kernel" {
                ds.push(cons.clone());
            }
            remaining.insert(n.clone(), ds);
        }
        while remaining.len() != 0 {
            let mut no_deps = Vec::new();
            for (n, ds) in remaining.iter() {
                let mut found = false;
                for d in ds.iter() {
                    if let Some(_) = remaining.get(d) {
                        found = true;
                        break;
                    }
                }
                if !found {
                    no_deps.push(n.clone());
                }
            }
            // TODO: We found a cycle. Error out properly...
            let len = no_deps.len();
            assert!(len > 0);
            for j in 0..len {
                remaining.remove(&no_deps[j]);
                tot_ord.push(no_deps[j].clone());
            }
        }

        let mut comps = BTreeMap::new();
        let mut rmap = BTreeMap::new();
        let mut id: ComponentId = 1;
        for c in tot_ord {
            comps.insert(id, c.clone());
            rmap.insert(c.clone(), id);
            id = id + 1;
        }

        // Order the address spaces so that they (and their
        // components) can be created parent address spaces first.
        let mut addrspc_comps = BTreeMap::new();
        let mut offset = 0;
        let mut comps_track_exclusive: HashSet<ComponentName> = comps.values().cloned().collect();
        for (_, a) in spec.address_spaces() {
            // a "root" of the AS hierarchy, recurs from there to do a DFS
            if a.parent.is_none() {
                addrspc_dfs_via_children(
                    &mut offset,
                    &mut addrspc_comps,
                    &a,
                    &spec.address_spaces(),
                );
            }
            // Remove components that are explicitly in address
            // spaces, so that we can track the *rest* that are in an
            // exclusive AS.
            for c in &a.components {
                comps_track_exclusive.remove(&c);
            }
        }
        let addrspc_exclusive = comps_track_exclusive.into_iter().collect();

        Ok(Box::new(CompTotOrd {
            ids: comps,
            rmap,
            addrspc_comps,
            addrspc_exclusive,
        }))
    }
}
