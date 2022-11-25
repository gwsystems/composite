use std::collections::{HashMap, HashSet};
use syshelpers::dump_file;
use toml;

use initargs::ArgsKV;
use passes::{
    AddrSpace, AddrSpaces, AddrSpcName, BuildState, Component, ComponentName, Dependency, Export,
    Library, SpecificationPass, SystemState, Transition,
};

#[derive(Debug, Deserialize)]
pub struct Dep {
    pub srv: String,
    pub interface: String,
    pub variant: Option<String>, // should only be used when srv == "kernel", i.e. a library variant
}

#[derive(Debug, Deserialize)]
pub struct Parameters {
    pub key: String,
    pub value: Option<String>,	// optional as we might provide simple keys without values.
    pub at: Option<String>,
}

#[derive(Debug, Deserialize)]
pub struct InterfaceVariant {
    pub interface: String,
    pub variant: Option<String>,
}

#[derive(Debug, Deserialize)]
pub struct TomlComponent {
    name: String,
    img: String,
    baseaddr: Option<String>,
    deps: Option<Vec<Dep>>,
    params: Option<Vec<Parameters>>,
    implements: Option<Vec<InterfaceVariant>>,
    initfs: Option<String>,
    constructor: String, // the booter
}

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
pub struct SysInfo {
    description: String, // comment
}

#[derive(Debug, Deserialize)]
pub struct TomlAddrSpace {
    name: String,
    components: Vec<String>, // names of components in the vas
    parent: Option<String>,  // which vas contains this one
}

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
pub struct TomlSpecification {
    system: SysInfo,
    components: Vec<TomlComponent>,
    address_spaces: Option<Vec<TomlAddrSpace>>, //aggregates: Vec<TomlComponent>  For components of components
}

impl Dep {
    pub fn get_name(&self) -> String {
        self.srv.clone()
    }
}

impl TomlComponent {
    fn update_options(&mut self) -> () {
        if self.deps.is_none() {
            let vs = Vec::new();
            self.deps = Some(vs);
        }

        if self.implements.is_none() {
            let vs = Vec::new();
            self.implements = Some(vs);
        }

        // TODO: should fill in the default variants here
    }

    pub fn deps(&self) -> &Vec<Dep> {
        self.deps.as_ref().unwrap()
    }

    pub fn interfaces(&self) -> &Vec<InterfaceVariant> {
        self.implements.as_ref().unwrap()
    }
}

impl TomlSpecification {
    fn comp(&self, cname: String) -> Option<&TomlComponent> {
        self.comps().iter().find(|c| c.name == cname)
    }

    fn comp_exists(&self, cname: String) -> bool {
        self.comp(cname).is_some()
    }

    // This MUST be called before anything else in the API
    fn validate(&mut self) -> Result<(), String> {
        self.comps_mut().iter_mut().for_each(|c| c.update_options());
        let mut fail = false;
        let mut err_accum = String::new();

        // Validate that we don't repeat components.
        //
        // TODO: This should really aggregate error strings with fold,
        // and return them as part of the function's Err, but I
        // appearantly can't rust.
        self.comps().iter().for_each(|c| {
            if 1 < self
                .comps()
                .iter()
                .fold(0, |n, c2| if c.name != c2.name { n } else { n + 1 })
            {
                err_accum.push_str(&format!(
                    "Error: TomlComponent name {} is defined multiple times.",
                    c.name
                ));
                fail = true;
            }
        });

        // Does the constructor resolve to a component? Does that
        // component have zero dependencies?
        //
        // TODO: allow multiple constructors, and ensure that their
        // relationship forms a tree
        if !self.comps().iter().fold(false, |found, c| {
            if found {
                return found;
            }
            if c.constructor != "kernel" {
                return found;
            }

            let ds: Vec<_> = c.deps().iter().filter(|d| d.srv != "kernel").collect();
            if ds.len() == 0 {
                true
            } else {
                err_accum.push_str(&format!(
                    "Error: Base constructor {} has dependencies.",
                    c.name
                ));
                false
            }
        }) {
            err_accum.push_str(&format!("Error: Appropriate system constructor not found."));
            fail = true;
        }

        // Check that 1. each address space includes components that
        // have been included in the components list, 2. that each
        // component is included maximum once in an address space, 3.
        // that each address spaces has a non-empty name, 4. that each
        // address space has a unique name, 5. that each parent of an
        // address space is a valid address space, and 6. that there
        // are no cycles with the parent relation.
        let mut addrspc_names = Vec::new();
        if self.ases().is_some() {
            for addrspc in self.ases().as_ref().unwrap().iter() {
                let mut referenced_components = Vec::new();

                // 1. Each address space component is a listed component?
                for as_c in &addrspc.components {
                    if !self
                        .comps()
                        .iter()
                        .fold(false, |found, c| found | (c.name == *as_c))
                    {
                        err_accum.push_str(&format!("Error: Address space \"{}\" includes component \"{}\" that is not found in the list of components.\n", addrspc.name, as_c));
                        fail = true;
                    }

                    // 2. Test that the components are only referenced
                    // a single time in address spaces.
                    if referenced_components.contains(&as_c) {
                        err_accum.push_str(&format!("Error: Address space \"{}\" includes component {} that is already found in another address space.\n", addrspc.name, as_c));
                        fail = true;
                    } else {
                        referenced_components.push(as_c);
                    }
                }

                // 3. ensure that address spaces have non-empty names.
                if addrspc.name.len() == 0 {
                    err_accum.push_str(&format!(
                        "Error: Address space has empty name. Must provide a non-empty name.\n"
                    ));
                    fail = true;
                    continue;
                }

                // 4. Make sure that address spaces have unique names.
                if addrspc_names.contains(&addrspc.name) {
                    err_accum.push_str(&format!("Error: Address space \"{}\" name is used by multiple address spaces; address spaces must have unique names.\n", addrspc.name));
                    fail = true;
                } else {
                    addrspc_names.push(addrspc.name.clone());
                }
            }

            let as_and_parents = self
                .ases()
                .as_ref()
                .unwrap_or(&Vec::new())
                .iter()
                .map(|a| (a.name.clone(), a.parent.clone()))
                .collect::<Vec<(String, Option<String>)>>();
            // 5. Check that the parent's, when provided, reference
            // named address spaces.
            for (addrspc, parent) in &as_and_parents {
                if let Some(p) = parent {
                    if !addrspc_names.contains(&p) {
                        err_accum.push_str(&format!("Error: Address space \"{}\" has parent \"{}\" where that name is not found among the names of address spaces.\n", addrspc, p));
                        fail = true;
                    }
                }
            }

            // 6. Last, we'll move address spaces to a list of the
            // parents (initialized with all address spaces without
            // any parents) from the initial list of address spaces.
            // We move over an AS only when its parent is in the list
            // of parents. If we cannot move over all components, then
            // there is a cycle.
            let mut parents_len = 0;
            let (mut parents, mut as_and_parents) = as_and_parents.iter().fold(
                (Vec::new(), Vec::new()),
                |(mut p, mut a), (asname, pname)| {
                    if pname.is_some() {
                        a.push((asname.to_string(), pname.clone()));
                    } else {
                        p.push(asname.to_string());
                    }
                    (p, a)
                },
            );
            // Iterate while we have more ASes to process, or until
            // there are no changes to the sets.
            while as_and_parents.len() > 0 && parents.len() != parents_len {
                parents_len = parents.len();
                let tmp = as_and_parents.iter().fold(
                    (parents, Vec::new()),
                    |(mut p, mut a), (asname, pname)| {
                        let mut moved = false;
                        if let Some(parentname) = pname {
                            if p.contains(parentname) {
                                p.push(asname.to_string());
                                moved = true;
                            }
                        }
                        if !moved {
                            a.push((asname.to_string(), pname.clone()));
                        }
                        (p, a)
                    },
                );
                parents = tmp.0;
                as_and_parents = tmp.1;
            }
            for (as_spc, p) in &as_and_parents {
                err_accum.push_str(&format!("Error: Address spaces \"{}\" and \"{}\" are involved in a cycle of parent dependencies. Cycles are not allowed.\n", as_spc, p.as_ref().unwrap()));
                fail = true;
            }
        }

        // Validate that all dependencies resolve to a defined
        // component, and that that dependency exports the depended on
        // interface.
        //
        // TODO: same as above...should aggregate error strings
        for c in self.comps() {
            for d in c.deps() {
                if d.get_name() == "kernel" {
                    if d.variant.is_none() {
                        err_accum.push_str(&format!(
                            "Error: Component {}'s dependency on the kernel for interface {} must specify a variant.",
                            c.name, d.interface
                        ));
                        fail = true;
                    }
                } else if let Some(ref s) = self.comp(d.get_name()) {
                    if s.interfaces()
                        .iter()
                        .find(|i| i.interface == d.interface)
                        .is_none()
                    {
                        err_accum.push_str(&format!(
                            "Error: Component {}'s dependency on {} is not exported by any depended on components.",
                            c.name, d.interface
                        ));
                        fail = true;
                    }
                } else {
                    err_accum.push_str(&format!(
                        "Error: Cannot find component referenced by dependency {} in component {}.",
                        d.get_name(),
                        c.name
                    ));
                    fail = true;
                }
            }
        }
        // validate that all directed params are to declared
        // components
        for c in self.comps() {
            if let Some(ref args) = c.params {
                for ia in args.iter() {
                    if let Some(ref name) = ia.at {
                        if !self.comp_exists(name.to_string()) {
                            err_accum.push_str(&format!("Error: Cannot find component referenced by directed params {} in component {}.",
                                                        name, c.name));
                            fail = true;
                        }
                    }
                }
            }
        }

        for c in self.comps() {
            if !self.comps().iter().fold(false, |accum, c2| {
                c.constructor == "kernel" || c.constructor == c2.name || accum
            }) {
                err_accum.push_str(&format!(
                    "Error: Component {}'s stated constructor ({}) is not a valid component.",
                    c.name, c.constructor
                ));
                fail = true;
            }
        }

        if self
            .comps()
            .iter()
            .filter(|c| c.constructor == "kernel")
            .count()
            != 1
        {
            err_accum.push_str(&format!("Error: the number of base constructors (with constructor = \"kernel\") is not singular."));
            fail = true;
        }

        if fail {
            Err(err_accum)
        } else {
            Ok(())
        }
    }

    pub fn comps(&self) -> &Vec<TomlComponent> {
        &self.components
    }

    pub fn ases(&self) -> &Option<Vec<TomlAddrSpace>> {
        &self.address_spaces
    }

    pub fn comps_mut(&mut self) -> &mut Vec<TomlComponent> {
        &mut self.components
    }

    pub fn parse(sysspec_path: &String) -> Result<TomlSpecification, String> {
        let conf = dump_file(&sysspec_path)?;
        // This is BRAIN DEAD.  There has to be a better way to get a str
        let cossys_pre: Result<TomlSpecification, _> =
            toml::from_str(String::from_utf8(conf).unwrap().as_str());

        if let Err(cs) = cossys_pre {
            let mut e = String::from("Error when parsing TOML:\n");
            e.push_str(&format!("{:?}", cs));
            return Err(e);
        }

        let mut cossys = cossys_pre.unwrap();
        if let Err(s) = cossys.validate() {
            let mut e = String::from("Error in system specification:\n");
            e.push_str(&format!("{}", s));
            return Err(e);
        }

        Ok(cossys)
    }
}

pub struct SystemSpec {
    ids: Vec<ComponentName>,
    components: HashMap<ComponentName, Component>,
    deps: HashMap<ComponentName, Vec<Dependency>>,
    libs: HashMap<ComponentName, Vec<Library>>,
    exports: HashMap<ComponentName, Vec<Export>>,
    address_spaces: HashMap<AddrSpcName, AddrSpace>,
}

// Helper functions to compute components in an address space, and
// those in all address spaces that descend from it. They assume the
// SystemSpec data-structures so that we can avoid redundantly
// computing the set of children address spaces (as that is part of
// creating SystemSpec::address_spaces.
fn addrspc_parent_closure<'a>(
    children: HashSet<&'a AddrSpcName>,
    all: &'a AddrSpaces,
) -> HashSet<&'a AddrSpcName> {
    let mut cs = children.clone();
    for (_, a) in all.iter() {
        if children.contains(&a.name) {
            let children_set: HashSet<&'a AddrSpcName> = a.children.iter().collect();
            cs = cs
                .union(&addrspc_parent_closure(children_set, all))
                .map(|c| *c)
                .collect();
        }
    }
    cs
}

// return value is the components in `addrspc`, and the set of
// components in address spaces that are descendants
fn addrspc_components<'a>(
    parent_name: &'a AddrSpcName,
    all: &'a AddrSpaces,
) -> (Vec<&'a ComponentName>, Vec<&'a ComponentName>) {
    // unwrap as we already validated the names
    let parent = all.get(parent_name).unwrap();
    let parent_comps = parent.components.iter().collect();

    let descendent_as = addrspc_parent_closure(parent.children.iter().collect(), all);
    let descendent_comps = descendent_as
        .iter()
        .fold(HashSet::new(), |agg, a| {
            let comps_as_set = all.get(*a).unwrap().components.iter().collect();
            agg.union(&comps_as_set).map(|c| *c).collect()
        })
        .into_iter()
        .collect();

    (parent_comps, descendent_comps)
}

impl Transition for SystemSpec {
    fn transition(s: &SystemState, _b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let spec_err = TomlSpecification::parse(&s.get_input());
        if let Err(e) = spec_err {
            return Err(e);
        }

        let spec = spec_err.unwrap();
        let ids = spec
            .comps()
            .iter()
            .map(|c| ComponentName::new(&c.name, &String::from("global")))
            .collect();
        // TODO: no libs currently considered
        let libs: HashMap<ComponentName, Vec<Library>> = HashMap::new();
        let mut components: HashMap<ComponentName, Component> = HashMap::new();
        let mut deps: HashMap<ComponentName, Vec<Dependency>> = HashMap::new();
        let mut exports: HashMap<ComponentName, Vec<Export>> = HashMap::new();

        for c in spec.comps().iter() {
            // TODO: assuming no use of "at" currently
            let ds: Vec<Dependency> = c
                .deps()
                .iter()
                .map(|d| Dependency {
                    server: ComponentName::new(&d.srv, &String::from("global")),
                    interface: d.interface.clone(),
                    // The variant is associated with the server,
                    // so we have to find the correct server,
                    // then the correct interface to find the
                    // variant. Note: the unwraps here are valid
                    // as they are checked in the validation step
                    variant: d.variant.clone().unwrap_or_else(|| {
                        spec.comp(d.srv.clone())
                            .unwrap()
                            .interfaces()
                            .iter()
                            .find(|i| i.interface == d.interface)
                            .unwrap()
                            .variant
                            .clone()
                            .unwrap_or_else(|| String::from("stubs"))
                    }),
                })
                .collect();

            let es = c
                .implements
                .as_ref()
                .unwrap_or(&Vec::new())
                .iter()
                .map(|e| Export {
                    interface: e.interface.clone(),
                    variant: e.variant.as_ref().unwrap_or(&"stubs".to_string()).clone(),
                })
                .collect();

            let sched_name = ds
                .iter()
                .find(|d| d.interface == "init" && d.variant != "kernel")
                .map(|d| d.server.clone())
                .unwrap_or_else(|| {
                    ComponentName::new(&String::from("kernel"), &String::from("global"))
                });

            let comp = Component {
                name: ComponentName::new(&c.name, &String::from("global")),
                constructor: ComponentName::new(&c.constructor, &String::from("global")),
                scheduler: sched_name,
                source: c.img.clone(),
                base_vaddr: c
                    .baseaddr
                    .as_ref()
                    .unwrap_or(&String::from("0x00400000"))
                    .clone(),
                params: c
                    .params
                    .as_ref()
                    .unwrap_or(&Vec::new())
                    .iter()
                    .map(|p| ArgsKV::new_key(p.key.clone(), p.value.as_ref().unwrap_or(&String::from("")).clone()))
                    .collect(),
                fsimg: c.initfs.clone(),
            };
            components.insert(ComponentName::new(&c.name, &String::from("global")), comp);
            deps.insert(ComponentName::new(&c.name, &String::from("global")), ds);
            exports.insert(ComponentName::new(&c.name, &String::from("global")), es);
        }

        // Create the address spaces structure
        let mut address_spaces = HashMap::new();
        if let Some(ref ases) = spec.ases() {
            for addrspc in ases {
                let name = addrspc.name.clone();
                let parent = addrspc.parent.clone();
                let components = addrspc
                    .components
                    .iter()
                    .map(|c| ComponentName::new(&c, &String::from("global")))
                    .collect();
                let children = ases
                    .iter()
                    .filter_map(|a| match a.parent {
                        Some(ref p) if name == *p => Some(a.name.clone()),
                        _ => None,
                    })
                    .collect();

                address_spaces.insert(
                    addrspc.name.clone() as AddrSpcName,
                    AddrSpace {
                        name,
                        components,
                        parent,
                        children,
                    },
                );
            }
        }

        let spec = Box::new(SystemSpec {
            ids,
            components,
            deps,
            libs,
            exports,
            address_spaces,
        });

        // Check that the address spaces are formed such that there
        // are no dependencies from components in parent (generally,
        // ancestor) address spaces to components in child address
        // spaces. This should be done in `validate`, but it requires
        // the children to be solved.
        let ases = spec.address_spaces();
        let mut errs = String::new();
        for (_, a) in ases {
            let (parent_comps, child_comps) = addrspc_components(&a.name, &ases);

            // Iterate through parent components, and ensure that they
            // do not depend on descendent address space components.
            for pc in &parent_comps {
                let backward_dep = spec
                    .deps_named(pc)
                    .iter()
                    .find(|&d| child_comps.contains(&&d.server));
                if backward_dep.is_some() {
                    let bd = backward_dep.unwrap();
                    errs.push_str(&format!(
			"Error: Dependency exists in address space \"{}\" from component \"{}\" to \"{}\" which is in a descendant address space; dependencies can only go from descendants to ancestors.",
			a.name, pc, bd.server));
                }
            }
        }
        if errs.len() != 0 {
            return Err(errs);
        }

        Ok(spec)
    }
}

impl SpecificationPass for SystemSpec {
    fn names(&self) -> &Vec<ComponentName> {
        &self.ids
    }

    fn component_named(&self, id: &ComponentName) -> &Component {
        &self.components.get(id).unwrap()
    }

    fn deps_named(&self, id: &ComponentName) -> &Vec<Dependency> {
        &self.deps.get(id).unwrap()
    }

    fn libs_named(&self, id: &ComponentName) -> &Vec<Library> {
        &self.libs.get(id).unwrap()
    }

    fn exports_named(&self, id: &ComponentName) -> &Vec<Export> {
        &self.exports.get(id).unwrap()
    }

    fn address_spaces(&self) -> &HashMap<AddrSpcName, AddrSpace> {
        &self.address_spaces
    }
}
