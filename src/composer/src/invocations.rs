use passes::{
    component, deps, BuildState, ComponentId, InvocationsPass, SInv, SystemState, TransitionIter,
};

use std::collections::HashSet;

pub struct Invocations {
    invs: Vec<SInv>,
}

fn sinvs_generate(id: &ComponentId, s: &SystemState) -> Result<Vec<SInv>, String> {
    let mut invs = Vec::new();
    let mut errors = String::from("");

    // find each undefined symbol
    for (sname, symbinfo) in s.get_objs_id(id).client_symbs() {
        let mut found = false;

        for d in deps(&s, &id) {

            let interface_toml = match s.get_spec().interface_funcs().get(&d.interface) {
                Some(interface) => interface,
                None => {
                    errors.push_str(&format!(
                        "Error: No interface TOML found for dependency {} in component {}\n",
                        d.interface, component(&s, &id).name
                    ));
                    return Err(errors);
                }
            };

            let mut function_exists = false;

            let mut func_access: Vec<String> = Vec::new();
            for func in &interface_toml.function {
                if func.name == *sname {
                    func_access = func.access.clone();
                    function_exists = true;
                    continue;
                }
            }

            if !function_exists {
                continue;
            }

            let srv_id = s
                .get_named()
                .ids()
                .iter()
                .filter_map(|(id, name)| if *name == d.server { Some(id) } else { None })
                .next()
                .unwrap();
            match s.get_objs_id(srv_id).server_symbs().get(sname) {
                Some(ref srv_symbs) => {
                    invs.push(SInv {
                        symb_name: sname.clone(),
                        client: id.clone(),
                        server: srv_id.clone(),
                        access: func_access,
                        virt_res_type: interface_toml.virt_resources.clone(),
                        c_fn_addr: symbinfo.func_addr.clone(),
                        c_callgate_addr: symbinfo.callgate_addr.clone(),
                        c_ucap_addr: symbinfo.ucap_addr.clone(),
                        s_fn_addr: srv_symbs.func_addr.clone(),
                        s_altfn_addr: srv_symbs.altfn_addr.clone(),
                    });
                    found = true;
                }
                None => continue,
            }
        }

        if !found {
            let mut aggdeps = String::from("");
            for d in deps(&s, &id) {
                aggdeps.push_str(&format!(" {}", d.server.clone()));
            }

            errors.push_str(&format!(
                r#"Error: Undefined dependency for unresolved function.  Component {} has an undefined function call to {} that is not satisfied by any of its dependencies (i.e. that function isn't provided by any of{}).\nReasons this could happen include:\n- None of the dependent servers provide that function. Make sure to include a function that exports an interface with {}.\n- The stubs in one of the servers don't properly export the function (search for __crt_s_{} in the server's exported symbols using `nm` or `objdump`) to see if this is the problem.\n- Every function in an interface must have a namespace matching the interface name (interface "pong" must only export functions named "pong_*"). Make sure that your functions are properly named in the interface.\n"#,
                component(&s, &id).name, sname, aggdeps, sname, sname));
        }
    }

    if errors.len() > 0 {
        return Err(errors);
    }

    Ok(invs)
}

fn sinvs_filter(invs: &mut Vec<SInv>, id: &ComponentId, s: &SystemState) -> Result<(), String> {
    let c = component(&s, id);

    // iterate over the virtual resource list
    for vr in &c.virt_res {
        let mut vr_access: HashSet<String> = HashSet::new();
        // Union the access options of all the instances of each virtual resource
        for inst in &vr.instances {
            for (_vr_inst_name, vr_inst_config) in inst {
                for access in &vr_inst_config.access {
                    vr_access.insert(access.clone()); 
                }
            }
        }

        // filter out the functions that don't match the virtual resource access options
        invs.retain(|inv| {
            if &inv.client == id && inv.virt_res_type == vr.vr_type {
                let access_matches = inv.access.iter().all(|access| vr_access.contains(access));
                return access_matches;
            }
            true
        });
    }

    Ok(())
}

impl TransitionIter for Invocations {
    fn transition_iter(
        id: &ComponentId,
        s: &SystemState,
        _b: &mut dyn BuildState,
    ) -> Result<Box<Self>, String> {
        let curr = s.get_named().ids().get(id).unwrap();
        let mut invs = Vec::new();

        for cid in s
            .get_named()
            .ids()
            .iter()
            .map(|(cid, _)| cid)
            .filter(|cid| {
                let c = component(&s, &cid);
                c.constructor == *curr
            })
        {
            // Should be true as constructor relationships should be
            // factored into the component id total order
            assert!(cid > id);
            let mut generated_invs = sinvs_generate(cid, s)?;
            sinvs_filter(&mut generated_invs, cid, s)?;
            invs.append(&mut generated_invs);
        }

        Ok(Box::new(Invocations { invs }))
    }
}

impl InvocationsPass for Invocations {
    fn invocations(&self) -> &Vec<SInv> {
        &self.invs
    }
}
