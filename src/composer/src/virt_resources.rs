use passes::{
    BuildState, VirtResPass, VirtResName, VirtResId, SystemState, Transition,
};
use std::collections::BTreeMap;

pub struct VirtResAnalysis {
    ids: BTreeMap<VirtResId, VirtResName>,
    rmap: BTreeMap<VirtResName, VirtResId>,
}

impl VirtResPass for VirtResAnalysis {
    fn ids(&self) -> &BTreeMap<VirtResId, VirtResName> {
        &self.ids
    }

    fn rmap(&self) -> &BTreeMap<VirtResName, VirtResId> {
        &self.rmap
    }
}

impl Transition for VirtResAnalysis {
    fn transition(s: &SystemState, _b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let mut ids = BTreeMap::new();
        let mut rmap = BTreeMap::new();

        // Iterate over the virtual resources in the system state
        let vr_map = s.get_spec().virtual_resources();

        for (_key, vr) in vr_map {
            let mut id_counter = 1;
            
            if vr.server == "chanmgr" {
                id_counter = 5;
            }

            // Iterate over each VirtRes and add a unique ID
            for virtres in &vr.resources {
                ids.insert(id_counter.to_string(), virtres.name.clone());
                rmap.insert(virtres.name.clone(), id_counter.to_string());              
                id_counter += 1;
            }
        }

        Ok(Box::new(VirtResAnalysis {
            ids,
            rmap
        }))
    }
}