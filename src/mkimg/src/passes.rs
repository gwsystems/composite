/// Define the structure of each of the passes of mkimg. This includes
/// passes for:
/// - Reading and parsing the specification file.
/// - Assigning components ids, and indexing structures by those ids (Component<ComponentName>)
/// - Calculate resource management relationships and dependencies w/ initargs addition. (Component<ComponentId>)
///
/// Each pass is codified in a trait so that it is implementation
/// polymorphic. Each of these traits has a set of methods that must
/// be defined: *transition*, which takes as input the previous state,
/// and computes the new state; and *transform*, which takes the
/// current state, and transforms it in some way (generating a new
/// state of the same type). Thus, the linker/loader is simply a set
/// of these phases composed together.
use std::collections::{BTreeMap, HashMap};

use initargs::ArgsKV;

pub struct SystemState {
    spec: String,

    parse: Option<Box<dyn SpecificationPass>>,
    named: Option<Box<dyn OrderedSpecPass>>,
    restbls: Option<Box<dyn ResPass>>,
    param: HashMap<ComponentId, Box<dyn InitParamPass>>,
    objs: HashMap<ComponentId, Box<dyn ObjectsPass>>,
    invs: HashMap<ComponentId, Box<dyn InvocationsPass>>,
    constructor: Option<Box<dyn ConstructorPass>>,
}

impl SystemState {
    pub fn new(spec: String) -> SystemState {
        SystemState {
            spec,
            parse: None,
            named: None,
            restbls: None,
            param: HashMap::new(),
            objs: HashMap::new(),
            invs: HashMap::new(),
            constructor: None,
        }
    }

    pub fn add_parsed(&mut self, p: Box<dyn SpecificationPass>) {
        self.parse = Some(p);
    }

    pub fn add_named(&mut self, n: Box<dyn OrderedSpecPass>) {
        self.named = Some(n);
    }

    pub fn add_restbls(&mut self, r: Box<dyn ResPass>) {
        self.restbls = Some(r);
    }

    pub fn add_params_iter(&mut self, id: &ComponentId, ip: Box<dyn InitParamPass>) {
        self.param.insert(*id, ip);
    }

    pub fn add_objs_iter(&mut self, id: &ComponentId, o: Box<dyn ObjectsPass>) {
        self.objs.insert(*id, o);
    }

    pub fn add_invs_iter(&mut self, id: &ComponentId, i: Box<dyn InvocationsPass>) {
        self.invs.insert(*id, i);
    }

    pub fn add_constructor(&mut self, c: Box<dyn ConstructorPass>) {
        self.constructor = Some(c);
    }

    pub fn get_input(&self) -> String {
        self.spec.clone()
    }

    pub fn get_spec(&self) -> &dyn SpecificationPass {
        &**(self.parse.as_ref().unwrap())
    }

    pub fn get_named(&self) -> &dyn OrderedSpecPass {
        &**(self.named.as_ref().unwrap())
    }

    pub fn get_restbl(&self) -> &dyn ResPass {
        &**(self.restbls.as_ref().unwrap())
    }

    pub fn get_param_id(&self, id: &ComponentId) -> &dyn InitParamPass {
        self.param.get(id).unwrap().as_ref()
    }

    pub fn get_objs_id(&self, id: &ComponentId) -> &dyn ObjectsPass {
        self.objs.get(id).unwrap().as_ref()
    }

    pub fn get_invs_id(&self, id: &ComponentId) -> &dyn InvocationsPass {
        self.invs.get(id).unwrap().as_ref()
    }

    pub fn computed_invs(&self) -> bool {
        !self.invs.is_empty()
    }

    pub fn get_constructor(&self) -> &dyn ConstructorPass {
        &**(self.constructor.as_ref().unwrap())
    }
}

// Note that none of this API does uniqueness checking: if a pass asks
// for the path to a file with a specific name, this won't check if
// that file already exists. Use unique names for files, and use the
// component-namespacing of names for per-component files.
pub trait BuildState {
    fn initialize(&mut self, name: &String, s: &SystemState) -> Result<(), String>; // must be called *before* the following functions
    fn file_path(&self, file: &String) -> Result<String, String>; // create a path in the build directory for a file
    fn comp_dir_path(&self, c: &ComponentId, state: &SystemState) -> Result<String, String>; // the component's object
    fn comp_file_path(
        &self,
        c: &ComponentId,
        file: &String,
        state: &SystemState,
    ) -> Result<String, String>; // path of a file associated with a component
    fn comp_obj_file(&self, c: &ComponentId, s: &SystemState) -> String; // name of the object file
    fn comp_obj_path(&self, c: &ComponentId, s: &SystemState) -> Result<String, String>; // the path to the component's object

    fn comp_build(&self, c: &ComponentId, state: &SystemState) -> Result<String, String>; // build the component, and return the path to the resulting object
    fn constructor_build(&self, c: &ComponentId, state: &SystemState) -> Result<String, String>; // build a constructor, including all components it is responsible for booting
}

// The following describes the means of transitioning the system
// between states, including iterative refinement of states on a
// per-component basis.

// Clean state transitions directly between entire states of
// processing.
pub trait Transition {
    fn transition(c: &SystemState, b: &mut dyn BuildState) -> Result<Box<Self>, String>;
}

// Transitions between states, component at a time. Ordered from most
// dependent components to most trusted.
//
// transition_iter will only be called for a component N when
// components 0..N-1 have been processed.
pub trait TransitionIter {
    fn transition_iter(
        id: &ComponentId,
        s: &SystemState,
        b: &mut dyn BuildState,
    ) -> Result<Box<Self>, String>;
}

// What follows is a description of each of the passes and their
// outputs.

pub type Interface = String;
pub type Variant = String;
pub type Library = String;
pub type VAddr = u64;

// unique identifier for a component
#[derive(Clone, Debug, PartialEq, Eq, Ord, PartialOrd, Hash)]
pub struct ComponentName {
    pub var_name: String,
    pub scope_name: String,
}

impl ComponentName {
    pub fn new(var_name: &String, scope_name: &String) -> ComponentName {
        ComponentName {
            var_name: var_name.clone(),
            scope_name: scope_name.clone(),
        }
    }
}

pub type ComponentId = u32;

// This structure carries throughout the entire process.
#[derive(Clone, Debug)]
pub struct Component {
    pub name: ComponentName, // How is the component identified? Component_{Name, Id}
    pub constructor: ComponentName, // the constructor that loads this component
    pub source: String,      // Where is the component source located?
    pub base_vaddr: String, // The lowest virtual address for the component -- could be hex, so not a VAddr
    pub params: Vec<ArgsKV>, // initialization parameters
    pub fsimg: Option<String>,
}

// Input/frontend pass taking the specification, and outputing the
// first intermediate representation. Expected to populate the
// Component structure.

#[derive(Clone, Debug)]
pub struct Dependency {
    pub server: ComponentName,
    pub interface: Interface,
    pub variant: Variant,
}

#[derive(Clone, Debug)]
pub struct Export {
    pub interface: Interface,
    pub variant: Variant,
}

pub trait SpecificationPass {
    fn names(&self) -> &Vec<ComponentName>;
    fn component_named(&self, id: &ComponentName) -> &Component;
    fn deps_named(&self, id: &ComponentName) -> &Vec<Dependency>;
    fn exports_named(&self, id: &ComponentName) -> &Vec<Export>;
    fn libs_named(&self, id: &ComponentName) -> &Vec<Library>;
}

// Integer namspacing pass. Convert the component variable names to
// component ids, and create a total order for the components based on
// their stated dependencies (lower ids are more trusted (more
// depended on).
pub trait OrderedSpecPass {
    fn ids(&self) -> &BTreeMap<ComponentId, ComponentName>;
}

// Helper access functions
pub fn component<'a>(s: &'a SystemState, id: &ComponentId) -> &'a Component {
    let name = s.get_named().ids().get(&id).unwrap();
    s.get_spec().component_named(name)
}

pub fn deps<'a>(s: &'a SystemState, id: &ComponentId) -> &'a Vec<Dependency> {
    let name = s.get_named().ids().get(&id).unwrap();
    s.get_spec().deps_named(name)
}

pub fn libs<'a>(s: &'a SystemState, id: &ComponentId) -> &'a Vec<Library> {
    let name = s.get_named().ids().get(&id).unwrap();
    s.get_spec().libs_named(name)
}

pub fn exports<'a>(s: &'a SystemState, id: &ComponentId) -> &'a Vec<Export> {
    let name = s.get_named().ids().get(&id).unwrap();
    s.get_spec().exports_named(name)
}

// Resource must be allocated and delegated to populate the resource
// tables for each component, and determine which is in charge of
// which resources.
pub type CapId = u32;
pub enum CapTarget {
    Ourselves,
    Client(ComponentId),
}
pub enum CapRes {
    CapTbl(CapTarget),
    PgTbl(CapTarget),
    Thd(CapTarget),
    Rcv(CapTarget), // ...
}
pub type CapTable = BTreeMap<CapId, CapRes>;

// Compute the resource table, and resource allocations for each
// component.
pub trait ResPass {
    // for now, ignoring page-table
    fn cap_tbl(&self, &ComponentId) -> &CapTable;
}

// The initparam, objects, and synchronous invocation passes are all
// per-object passes as there are dependencies between the
// components. When we create the initial arguments for objects, we
// can create the binaries, then we can get the sinv addresses. With
// this, we can create a constructor's sinv init parameters data, but in
// doing so, we have to compile and create a new object. Thus we must
// decouple the client's object creation from that of the constructor.
//
// Note that the *structures* that implement this trait are
// per-component, so we don't need per-component ids as arguments
// here.

pub trait InitParamPass {
    fn param_list(&self) -> &Vec<ArgsKV>; // the arguments
    fn param_prog(&self) -> &String; // path to the parameter program
    fn param_fs(&self) -> &Option<String>; // path to the file-system image (tarball)
}

// The object pass creates the binary for the component, and
// introspects into it to gather all the relevant cos runtime symbols.

pub struct ClientSymb {
    pub func_addr: VAddr,
    pub ucap_addr: VAddr,
}

pub struct CompSymbs {
    pub entry: VAddr,
    pub comp_info: VAddr,
}

pub trait ObjectsPass {
    fn client_symbs(&self) -> &HashMap<String, ClientSymb>;
    fn server_symbs(&self) -> &HashMap<String, VAddr>;
    fn comp_symbs(&self) -> &CompSymbs;
    fn comp_path(&self) -> &String;
}

// The invocations pass retrieves the synchronous invocation meta-data
// from each component. For the reasons listed above in the object
// creation pass, this is a per-component pass.

// All of the information required to create all of the synchronous
// invocations between a client and server for a single function
#[derive(Debug)]
pub struct SInv {
    pub symb_name: String,
    pub client: ComponentId,
    pub server: ComponentId,
    pub c_fn_addr: VAddr,
    pub c_ucap_addr: VAddr,
    pub s_fn_addr: VAddr,
}

pub trait InvocationsPass {
    fn invocations(&self) -> &Vec<SInv>;
}

// The Link pass is the final pass and it creates the final bootable
// image.

pub trait ConstructorPass {
    fn image_path(&self) -> &String;
}
