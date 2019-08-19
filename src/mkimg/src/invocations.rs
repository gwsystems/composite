use passes::{InvocationsPass, SInv, ComponentId, SystemState, BuildState, TransitionIter, deps, libs, exports, component};

pub struct Invocations {
    invs: Vec<SInv>
}

fn sinvs_generate(id: &ComponentId, s: &SystemState) -> Result<Vec<SInv>, String> {
    let mut invs = Vec::new();
    let mut errors = String::from("");
    let spec = s.get_spec();

    // find each undefined symbol
    for (sname, symbinfo) in s.get_objs_id(id).client_symbs() {
        let mut found = false;

        for d in deps(&s, &id) {
            // find the correct dependency (whose interface
            // prefixes the symbol)
            if !sname.starts_with(&d.interface) {
                continue;
            }

            let srv_id = s.get_named().ids().iter().filter_map(|(id, name)| {
                if *name == d.server {
                    Some(id)
                } else {
                    None
                }
            }).next().unwrap();
            match s.get_objs_id(srv_id).server_symbs().get(sname) {
                Some(ref srv_symbs) => {
                    invs.push(SInv {
                        symb_name: sname.clone(),
                        client: id.clone(),
                        server: srv_id.clone(),
                        c_fn_addr: symbinfo.func_addr.clone(),
                        c_ucap_addr: symbinfo.ucap_addr.clone(),
                        s_fn_addr: *srv_symbs.clone()
                    });
                    found = true;
                },
                None => continue
            }
        }

        if !found {
            let mut aggdeps = String::from("");
            for d in deps(&s, &id) {
                aggdeps.push_str(&format!("{:?}", d.server.clone()));
            }

            errors.push_str(&format!(
                "Error: Undefined dependency for unresolved function.  Component {:?} has an undefined function call to {} that is not satisfied by any of its dependencies (i.e. that function isn't provided by any of {}).\n",
                component(&s, &id).name, sname, aggdeps));
        }
    }

    if errors.len() > 0 {
        return Err(errors);
    }

    Ok(invs)
}

impl TransitionIter for Invocations {
    fn transition_iter(id: &ComponentId, s: &SystemState, b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let curr = s.get_named().ids().get(id).unwrap();
        let mut invs = Vec::new();

        for cid in s.get_named().ids().iter().map(|(cid, _)| cid).filter(|cid| {
            let c = component(&s, &cid);
            c.constructor == *curr
        })
        {
            // Should be true as constructor relationships should be
            // factored into the component id total order
            assert!(cid > id);
            invs.append(&mut (sinvs_generate(cid, s)?));
        }

        Ok(Box::new(Invocations {
            invs
        }))
    }
}

impl InvocationsPass for Invocations {
    fn invocations(&self) -> &Vec<SInv> {
        &self.invs
    }
}
