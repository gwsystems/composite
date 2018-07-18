use cossystem::{CosSystem,Component};
use std::collections::BTreeMap;

// Interact with the composite build system to "seal" the components.
// This requires linking them with all dependencies, and with libc,
// and making them executable programs (i.e. no relocatable objects).
//
// Create the build context for a component derived from its
// specification the sysspec, including 1. the interfaces it
// exports, 2. the interface dependencies it relies on, and 3. the
// libraries it requires.  The unexpected part of this is that
// each interface can have its own dependencies and library
// requirements, thus this must return the transitive closure over
// those dependencies.
//
// Note that the Makefiles of components and interfaces include a
// specification of all of this.  FIXME: We should check the
// specification in the syspec against these local (and
// compile-checked) specifications, and bomb out with an error if
// they don't match.  For now, a disparity will result in compiler
// errors, instead of errors here.  Thus, FIXME.
//
// The goal of the context is to build up all of this information,
// then call `make component` with the correct make variables
// initialized.  These include:
//
// - COMP_INTERFACES - list of space separated exported
//   interfaces/variant pairs. For example "pong/stubs" for default,
//   or "pong/log" for a log variant
// - COMP_IFDEPS - list of space separated interface dependencies and
//   variants, again specified as "if/variant"
// - COMP_LIBS - list of space separated library dependencies
// - COMP_INTERFACE - this component's interface directory
// - COMP_NAME - which component implementation to use
// - COMP_VARNAME - the name of the component's variable in the sysspec
//
// In the end, this should result in a command line for each component
// along these (artificial) lines:
//
// `make COMP_INTERFACES="pong/log" COMP_IFDEPS="capmgr/stubs sched/lock" COMP_LIBS="ps heap" COMP_INTERFACE=pong COMP_NAME=pingpong COMP_VARNAME=pongcomp component`
//
// ...which should output the executable pong.pingpong.pongcomp in the
// build directory which is the "sealed" version of the component that
// is ready for loading.

// Get the path to the component implementation directory. Should
// probably derive this from an environmental var passed in at compile
// time by the surrounding build system.
pub fn comps_base_path() -> String {
    let mut path = String::from(env!("PWD"));

    // Construct the path to the src directory
    path.push_str("/../");
    path.push_str("components/implementation/");
    path
}

pub fn interface_path(interface: String, variant: Option<String>) -> String {
    let mut path = String::from(env!("PWD"));

    path.push_str("../");
    path.push_str("components/interface/");
    path.push_str(&interface);
    if let Some(v) = variant {
        path.push_str(&v);
    }
    path
}

// Get the path to a component object via its name.  <if>.<name>
// resolves to src/components/implementation/if/name/if.name.o
pub fn comp_path(comp_base: &String, img: &String) -> String {
    let mut obj_path = comp_base.clone();
    let mut obj_str = img.clone();
    let mut obj_subpath = obj_str.replace(".", "/");

    obj_subpath.push_str("/");
    obj_path.push_str(&obj_subpath);
    obj_path.push_str(&obj_str);
    obj_path.push_str(".o");
    obj_path
}

#[derive(Debug)]
struct ComponentContext {
    comp_name: String,          // the component implementation name
    comp_if: String,            // component interface directory
    var_name: String,           // the sysspec name
    make_cmd: Option<String>,
    interface_exports: Vec<(String, String)>, // interface/variant pairs
    interface_deps: Vec<(String, String, String)>,    // interface/server/variant pairs derived from server

    interface_servers: Vec<(String, String)>, // server/interface pairs to help deriving interface_deps
    library_deps: Vec<String>
}

pub struct BuildContext {
    comps: BTreeMap<String, ComponentContext> // component variable name and context
}

fn comp_interface_name(img: &String) -> (String, String) {
    let obj_str = img.clone();
    let if_name: Vec<&str> = obj_str.split('.').collect();
    assert!(if_name.len() == 2);

    (if_name[0].clone().to_string(), if_name[1].clone().to_string())
}

impl ComponentContext {
    // Build up context for the componet based on information only
    // from its individual specification.  Another phase is necessary
    // to build the rest of the context from the global specification
    // considering *all* components (see comp_global_context in
    // BuildContext).
    pub fn new(comp: &Component) -> ComponentContext {
        let (interface, name) = comp_interface_name(&comp.img());
        let mut compctxt = ComponentContext {
            comp_name: name,
            comp_if: interface.clone(),
            var_name: comp.name().clone(),
            make_cmd: None,
            interface_exports: Vec::new(),
            interface_deps: Vec::new(),
            interface_servers: Vec::new(),
            library_deps: Vec::new()
        };
        let mut found_if = false;

        // populate all of the initial values from the system specification
        for ifs in comp.interfaces().iter() {
            // populate the interfaces that this component exports
            if ifs.interface == interface {
                found_if = true;
            }
            compctxt.interface_exports.push((ifs.interface.clone(), match ifs.variant {
                Some(ref v) => v.clone(),
                None => String::from("stubs") // default variant for *every* interface
            }));

        }
        // if not specified in the explicit dependencies, add the default
        if !found_if {
            compctxt.interface_exports.push((interface, String::from("stubs")));
        }

        // populate the dependencies...only copied here to keep all info in one place
        for dep in comp.deps().iter() {
            compctxt.interface_servers.push((dep.interface.clone(), dep.srv.clone()));
        }

        compctxt
    }

    pub fn validate_deps(&self) -> Result<(), String> {
        if self.interface_deps.len() != self.interface_servers.len() {
            Err(String::from(format!("Component {} (implementation: {}) has dependencies that are not satisfied by stated dependencies:\n\tDependencies {:?}\n\tProvided {:?}", self.var_name, self.comp_name, self.interface_servers, self.interface_deps)))
        } else {
            Ok(())
        }
    }
}

impl BuildContext {
    pub fn new(comps: &Vec<Component>) -> BuildContext {
        let mut ctxt = BTreeMap::new();

        // set up all of the component's interface and variant information...
        for c in comps.iter() {
            let mut c_ctxt = ComponentContext::new(&c);

            // for each of the servers in the component's interface,
            // find the server, then find the variant for that
            // interface.  Use the cosspec structures for this to
            // avoid double reference to the mutable new component.
            for dep in c.deps().iter() {
                for srv in comps.iter() {
                    if *srv.name() == dep.srv {
                        let mut found = false;
                        for inter in srv.interfaces().iter() {
                            if inter.interface == dep.interface {
                                // note that the `unwrap` is safe
                                // since we completed all variants
                                // when creating the component
                                // context.
                                c_ctxt.interface_deps.push((inter.interface.clone(), dep.srv.clone(), inter.variant.clone().unwrap()));
                                found = true;
                                break;
                            }
                        }
                        // This is complete trash.  Should be able to
                        // centralize this logic.
                        if !found {
                            let (interface, name) = comp_interface_name(&srv.img());
                            if interface == dep.interface {
                                c_ctxt.interface_deps.push((interface, dep.srv.clone(), String::from("stubs")));
                            }
                        }
                        break;
                    }
                }
            }
            ctxt.insert(c.name().clone(), c_ctxt);
        }

        BuildContext {
            comps: ctxt
        }
    }

    pub fn validate_deps(&self) -> Result<(), String> {
        // aggregate the errors
        let errs: Vec<_> = self.comps.iter().filter_map(|(_, comp)| match comp.validate_deps() {
            Ok(_) => None,
            Err(s) => Some(s)
        }).collect();
        if errs.len() == 0 {
            Ok(())
        } else {
            Err(errs.iter().fold(String::from(""), |mut agg, e| { agg.push_str(e); agg }))
        }
    }

    fn calculate_make_cmds(&mut self) -> () {
        for (n, mut c) in self.comps.iter_mut() {
            let mut cmd = String::from("make ");
            let mut ifs = c.interface_exports.iter().fold(String::from("COMP_INTERFACES=\""), |accum, (interf, var)| {
                let mut ifpath = accum.clone();
                ifpath.push_str(&interf.clone());
                ifpath.push_str("/");
                ifpath.push_str(&var.clone());
                ifpath.push_str(" ");
                ifpath
            });
            ifs.push_str("\" ");
            let mut if_deps = c.interface_deps.iter().fold(String::from("COMP_IFDEPS=\""), |accum, (interf, srv, var)| {
                let mut ifpath = accum.clone();
                ifpath.push_str(&interf.clone());
                ifpath.push_str("/");
                ifpath.push_str(&var.clone());
                ifpath.push_str(" ");
                ifpath
            });
            if_deps.push_str("\" ");
            let libs = String::from("");
            let mut path_if = String::from("COMP_INTERFACE=");
            path_if.push_str(&c.comp_if);
            path_if.push_str(" ");
            let mut path_name = String::from("COMP_NAME=");
            path_name.push_str(&c.comp_name);
            path_name.push_str(" ");
            let mut path_var = String::from("COMP_VARNAME=");
            path_var.push_str(&c.var_name);

            cmd.push_str(&ifs);
            cmd.push_str(&if_deps);
            cmd.push_str(&libs);
            cmd.push_str(&path_if);
            cmd.push_str(&path_name);
            cmd.push_str(&path_var);
            cmd.push_str(" component");

            println!("{}", cmd);
            c.make_cmd = Some(cmd);
        }
    }

    pub fn build_components(&mut self) -> () {
        self.calculate_make_cmds()
    }
}
