use cossystem::{CosSystem,Component};
use std::collections::BTreeMap;
use syshelpers::{exec_pipeline,reset_dir};
use std::process;
use tar::Builder;
use std::fs::File;
use std::io::prelude::*;
use compose::Compose;
use booter::{booter_serialize_args, booter_tar_dirkey};

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
// - COMP_BASEADDR - the base address of .text for the component
// - COMP_INITARGS_FILE - the path to the generated initial arguments .c file
// - COMP_TAR_FILE - the path to an initargs tarball to compile into the component
//
// In the end, this should result in a command line for each component
// along these (artificial) lines:
//
// `make COMP_INTERFACES="pong/log" COMP_IFDEPS="capmgr/stubs sched/lock" COMP_LIBS="ps heap" COMP_INTERFACE=pong COMP_NAME=pingpong COMP_VARNAME=pongcomp component`
//
// ...which should output the executable pong.pingpong.pongcomp in the
// build directory which is the "sealed" version of the component that
// is ready for loading.

#[derive(Debug)]
struct ComponentContext {
    comp_name: String,          // the component implementation name
    comp_if: String,            // component interface directory
    var_name: String,           // the sysspec name
    make_cmd: Option<String>,
    base_addr: String,          // base address of component in hex
    interface_exports: Vec<(String, String)>, // interface/variant pairs
    interface_deps: Vec<(String, String, String)>,    // interface/server/variant pairs derived from server

    interface_servers: Vec<(String, String)>, // server/interface pairs to help deriving interface_deps
    library_deps: Vec<String>
}

pub struct BuildContext {
    comps: BTreeMap<String, ComponentContext>, // component variable name and context
    booter: ComponentContext,
    builddir: String
}

fn comp_interface_name(img: &String) -> (String, String) {
    let obj_str = img.clone();
    let if_name: Vec<&str> = obj_str.split('.').collect();
    assert!(if_name.len() == 2);

    (if_name[0].clone().to_string(), if_name[1].clone().to_string())
}

impl ComponentContext {
    pub fn new_minimal(interface: &String, name: &String, varname: &String) -> ComponentContext {
        ComponentContext {
            comp_name: name.clone(),
            comp_if: interface.clone(),
            var_name: varname.clone(),
            base_addr: String::from("0x00400000"),
            make_cmd: None,
            interface_exports: Vec::new(),
            interface_deps: Vec::new(),
            interface_servers: Vec::new(),
            library_deps: Vec::new()
        }
    }

    // Build up context for the component based on information only
    // from its individual specification.  Another phase is necessary
    // to build the rest of the context from the global specification
    // considering *all* components (see comp_global_context in
    // BuildContext).
    pub fn new(comp: &Component) -> ComponentContext {
        let (interface, name) = comp_interface_name(&comp.img());
        let mut compctxt = ComponentContext::new_minimal(&interface, &name, &comp.name());
        if let Some(ref s) = comp.baseaddr() {
            compctxt.base_addr = s.clone()
        }
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
        if !found_if && interface != "tests" && interface != "no_interface" {
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

    pub fn deps(&self) -> Vec<(String, String)> {
        // return the inteface, and dependency, but not the variant
        self.interface_deps.iter().map(|(i, s, v)| (s.clone(), i.clone())).collect()
    }
}

fn comp_obj_name(interface: &String, comp_impl: &String, varname: &String) -> String {
    format!("{}.{}.{}", &interface, &comp_impl, &varname)
}

fn comp_build_obj_path(builddir: &String, interface: &String, comp_impl: &String, varname: &String) -> String {
    format!("{}{}", &builddir, comp_obj_name(interface, comp_impl, varname))
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

        let builddir = format!("{}/cos_build_{}/", env!("PWD"), process::id());

        BuildContext {
            comps: ctxt,
            // hard-code the booter for now:
            booter: ComponentContext::new_minimal(&String::from("no_interface"), &String::from("llbooter"), &String::from("booter")),
            builddir: builddir
        }
    }

    // Return (server, interface) dependency pairs
    pub fn comp_deps(&self, name: &String) -> Option<Vec<(String, String)>> {
        match self.comps.get(name) {
            Some(c) => Some(c.deps()),
            None => None
        }
    }

    pub fn comp_obj_name(&self, varname: &String) -> Result<String, String> {
        match self.comps.get(varname) {
            Some(ref c) => Ok(comp_obj_name(&c.comp_if, &c.comp_name, &c.var_name)),
            None => Err(format!("Error: Could not find component {}.\n", varname))
        }
    }

    pub fn comp_obj_path(&self, varname: &String) -> Result<String, String> {
        match self.comps.get(varname) {
            Some(ref c) => {
                assert!(&c.var_name == varname);
                Ok(comp_build_obj_path(&self.builddir, &c.comp_if, &c.comp_name, &c.var_name))
            },
            None => return Err(format!("Error: Could not find component {}.\n", varname))
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

    fn refresh_build_dir(&mut self) -> () {
        // clear out the build directory, or use the current directory if we can't
        let tmpdir = match(reset_dir(self.builddir.clone())) {
            Ok(_) => self.builddir.clone(),
            Err(_) => {
                self.builddir = format!("{}/", env!("PWD"));
                self.builddir.clone()
            }
        };
    }

    fn comp_gen_make_cmd(c: &ComponentContext, builddir: &String, initargsfile: Option<String>, tarfile: Option<String>) -> String {
        let (_, mut ifs) = c.interface_exports.iter().fold((true, String::from("")), |(first, accum), (interf, var)| {
            let mut ifpath = accum.clone();
            if !first {
                ifpath.push_str(" ");
            }
            ifpath.push_str(&interf.clone());
            ifpath.push_str("/");
            ifpath.push_str(&var.clone());
            (false, ifpath)
        });
        let (_, mut if_deps) = c.interface_deps.iter().fold((true, String::from("")), |(first, accum), (interf, srv, var)| {
            let mut ifpath = accum.clone();
            if !first {
                ifpath.push_str(" ");
            }
            ifpath.push_str(&interf.clone());
            ifpath.push_str("/");
            ifpath.push_str(&var.clone());
            (false, ifpath)
        });

        let mut optional_cmds = String::from("");
        if let Some(s) = initargsfile {
            optional_cmds.push_str(&format!("COMP_INITARGS_FILE={} ", s));
        }
        if let Some(s) = tarfile {
            optional_cmds.push_str(&format!("COMP_TAR_FILE={} ", s));
        }

        let cmd = format!(r#"make -C ../ COMP_INTERFACES="{}" COMP_IFDEPS="{}" COMP_INTERFACE={} COMP_NAME={} COMP_VARNAME={} COMP_OUTPUT={} COMP_BASEADDR={} {} component"#,
                          ifs, if_deps, &c.comp_if, &c.comp_name, &c.var_name, &comp_build_obj_path(&builddir, &c.comp_if, &c.comp_name, &c.var_name), &c.base_addr, &optional_cmds);

        println!("Make command for component {}: {}", c.var_name, cmd);
        cmd
    }

    fn calculate_make_cmds(&mut self) -> () {
        self.refresh_build_dir();

        for (n, mut c) in self.comps.iter_mut() {
            c.make_cmd = Some(BuildContext::comp_gen_make_cmd(&c, &self.builddir, None, None));
        }
    }

    pub fn build_components(&mut self) -> () {
        self.calculate_make_cmds();
        for (n, c) in self.comps.iter() {
            let mut cmd = String::from("");
            cmd.push_str(c.make_cmd.as_ref().unwrap());
            let (out, err) = exec_pipeline(vec![cmd]);
            println!("Component {} compilation output:
{}\nComponent compilation errors:
{}", n, out, err);
        }
    }

    pub fn gen_booter(&self, compose: &Compose) -> () {
        let tar_path = format!("{}booter_bins.tar", self.builddir);
        let initargs_path = format!("{}booter_initargs.c", self.builddir);

        // populate the tarball for the booter
        let file = File::create(&tar_path).unwrap();
        let mut ar = Builder::new(file);
        for (n, c) in self.comps.iter() {
            let path = comp_build_obj_path(&self.builddir, &c.comp_if, &c.comp_name, &c.var_name);
            let name = comp_obj_name(&c.comp_if, &c.comp_name, &c.var_name);
            let mut f = File::open(path).unwrap(); //  should not fail: we just build this
            ar.append_file(format!("{}/{}", booter_tar_dirkey(), name), &mut f).unwrap(); // FIXME: error handling
        }

        let mut initargs_file = File::create(&initargs_path).unwrap();
        initargs_file.write_all(booter_serialize_args(&compose).as_bytes()).unwrap();

        let cmd = BuildContext::comp_gen_make_cmd(&self.booter, &self.builddir, Some(initargs_path), Some(tar_path));
        let (out, err) = exec_pipeline(vec![cmd]);
        println!("Booter compilation output:
{}\nComponent compilation errors:
{}", out, err);
    }
}

// Get the path to the component implementation directory. Should
// probably derive this from an environmental var passed in at compile
// time by the surrounding build system.
pub fn comps_base_path() -> String {
    format!("{}/../components/implementation/", env!("PWD"))
}

pub fn interface_path(interface: String, variant: Option<String>) -> String {
    format!("{}/../components/interface/{}/{}/", env!("PWD"), interface, match variant {
        Some(v) => v.clone(),
        None => String::from("stubs")
    })
}

// Get the path to a component object via its name.  <if>.<name>
// resolves to src/components/implementation/if/name/if.name.o
pub fn comp_path(img: &String) -> String {
    format!("{}{}/{}.o", comps_base_path(), img.clone().replace(".", "/"), img.clone())
}
