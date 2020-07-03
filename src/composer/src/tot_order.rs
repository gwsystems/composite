use passes::{BuildState, ComponentId, ComponentName, OrderedSpecPass, SystemState, Transition};
use std::collections::BTreeMap;

pub struct CompTotOrd {
    ids: BTreeMap<ComponentId, ComponentName>,
    rmap: BTreeMap<ComponentName, ComponentId>,
}

impl OrderedSpecPass for CompTotOrd {
    fn ids(&self) -> &BTreeMap<ComponentId, ComponentName> {
        &self.ids
    }

    fn rmap(&self) -> &BTreeMap<ComponentName, ComponentId> {
        &self.rmap
    }
}

impl Transition for CompTotOrd {
    fn transition(s: &SystemState, b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let spec = s.get_spec();

        // Find a total order of components based on the dependency
        // relation...
        let mut tot_ord = Vec::new();
        // which components are still remaining without an placement
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

        Ok(Box::new(CompTotOrd {
            ids: comps,
            rmap: rmap,
        }))
    }
}
