use std::collections::HashMap;
use syshelpers::dump_file;
use toml;

use initargs::ArgsKV;
use passes::{
    BuildState, Component, ComponentName, Dependency, Export, Library, SpecificationPass,
    SystemState, Transition,
};

#[derive(Debug, Deserialize)]
pub struct Dep {
    pub srv: String,
    pub interface: String,
    pub variant: Option<String>, // should only be used when srv == "kernel", i.e. a library variant
}

#[derive(Debug, Deserialize)]
pub struct Parameters {
    pub name: String,
    pub value: String,
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
pub struct SysInfo {
    description: String, // comment
}

#[derive(Debug, Deserialize)]
pub struct TomlSpecification {
    system: SysInfo,
    components: Vec<TomlComponent>,
    //aggregates: Vec<TomlComponent>  For components of components
}

impl Dep {
    pub fn new(name: String) -> Dep {
        Dep {
            srv: name.clone(),
            interface: "".to_string(),
            variant: None,
        }
    }

    pub fn get_name(&self) -> String {
        self.srv.clone()
    }
}

impl TomlComponent {
    pub fn new(name: String, img: String) -> TomlComponent {
        TomlComponent {
            name: name,
            img: img,
            baseaddr: None,
            deps: Some(Vec::new()),
            implements: None,
            params: None,
            initfs: None,
            constructor: String::from(""),
        }
    }

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

    pub fn img(&self) -> &String {
        &self.img
    }

    pub fn name(&self) -> &String {
        &self.name
    }

    pub fn baseaddr(&self) -> &Option<String> {
        &self.baseaddr
    }

    pub fn params(&self) -> &Option<Vec<Parameters>> {
        &self.params
    }

    pub fn initfs(&self) -> &Option<String> {
        &self.initfs
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
                            c.name, d.get_name()
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

    pub fn comps_mut(&mut self) -> &mut Vec<TomlComponent> {
        &mut self.components
    }

    pub fn resolve_dep(&self, name: String) -> Option<&TomlComponent> {
        self.components.iter().filter(|c| name == c.name).next()
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
            e.push_str(&format!("{:?}", s));
            return Err(e);
        }

        Ok(cossys)
    }

    pub fn empty() -> TomlSpecification {
        TomlSpecification {
            system: SysInfo {
                description: String::from(""),
            },
            components: Vec::new(),
        }
    }
}

pub struct SystemSpec {
    ids: Vec<ComponentName>,
    components: HashMap<ComponentName, Component>,
    deps: HashMap<ComponentName, Vec<Dependency>>,
    libs: HashMap<ComponentName, Vec<Library>>,
    exports: HashMap<ComponentName, Vec<Export>>,
}

impl SystemSpec {
    fn new() -> Self {
        SystemSpec {
            ids: Vec::new(),
            components: HashMap::new(),
            deps: HashMap::new(),
            libs: HashMap::new(),
            exports: HashMap::new(),
        }
    }
}

impl Transition for SystemSpec {
    fn transition(s: &SystemState, b: &mut dyn BuildState) -> Result<Box<Self>, String> {
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
            let ds:Vec<Dependency> = c
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
                    variant: d
                        .variant
                        .clone()
                        .unwrap_or_else(|| {
                            spec.comp(d.srv.clone())
                                .unwrap()
                                .interfaces()
                                .iter()
                                .find(|i| i.interface == d.interface)
                                .unwrap()
                                .variant
                                .clone()
                                .unwrap_or_else(|| String::from("stubs"))
                        })
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
                .unwrap_or_else(|| ComponentName::new(&String::from("kernel"), &String::from("global")));

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
                    .map(|p| ArgsKV::new_key(p.name.clone(), p.value.clone()))
                    .collect(),
                fsimg: c.initfs.clone(),
            };
            components.insert(ComponentName::new(&c.name, &String::from("global")), comp);
            deps.insert(ComponentName::new(&c.name, &String::from("global")), ds);
            exports.insert(ComponentName::new(&c.name, &String::from("global")), es);
        }

        Ok(Box::new(SystemSpec {
            ids,
            components,
            deps,
            libs,
            exports,
        }))
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
}
