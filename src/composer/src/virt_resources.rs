use passes::{
    BuildState, VirtResPass, VirtResName, VirtResWitID, VirtualResource, SystemState, Transition,
};
use std::collections::BTreeMap;

use cossystem::Clients;

pub struct VirtResAnalysis {
    virt_res_with_id: BTreeMap<VirtResName,VirtualResource>,
}

impl VirtResPass for VirtResAnalysis {
    fn virt_res_with_id(&self) -> &BTreeMap<VirtResName,VirtualResource> {
        &self.virt_res_with_id
    }
}

impl Transition for VirtResAnalysis {
    fn transition(s: &SystemState, _b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let mut virt_res_with_id = BTreeMap::new();
        let mut id_counter = 1;

        // Iterate over the virtual resources in the system state
        let vr_map = s.get_spec().virtual_resources();

        for (_key, vr) in vr_map {
            let mut virt_res_list = Vec::new();

            if vr.server == "chanmgr" {
                id_counter = 5;
            }
            
            // Iterate over each VirtRes and add a unique ID
            for virtres in &vr.resources {
                let virt_resource_id = id_counter.to_string();
                id_counter += 1;

                // Create a new VirtResWitID with the generated ID
                let virt_res_with_id = VirtResWitID {
                    virt_resource_id,
                    param: virtres.param.clone(),
                    clients: virtres.clients.iter().map(|client| Clients {
                        comp: client.comp.clone(),
                        access: client.access.clone(),
                        name: client.name.clone(),
                        symbol: client.symbol.clone(),
                    }).collect(),                };

                virt_res_list.push(virt_res_with_id);
            }

            // Create a new VirtualResource with the list of VirtResWitID
            let virtual_resource = VirtualResource {
                name: vr.name.clone(),
                server: vr.server.clone(),
                resources: virt_res_list,
            };

            virt_res_with_id.insert(vr.name.clone(), virtual_resource);
        }

        Ok(Box::new(VirtResAnalysis {
            virt_res_with_id,
        }))
    }
}