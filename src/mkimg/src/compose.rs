// This file gathers together all of the information from:
//
// 1. the system specification,
// 2. a processing of that specification that infers all of the data
//     not provided by the specification itself
// 3. the component objects themselves post-linking
//
// It orchestrates the linking of all components, and derives all of
// the information needed by the booter to link the system graph
// together (e.g. all of the sinv information).
//
// It has to work around an annoying Rust issue: We need to read in the
// binaries for each component, then process them as Elf binaries.
// However, the entire data-structure cannot be completed at once
// since it involved dynamic creation via the system specification.
// Thus the explicit lifetime of the binaries prevents the Elf
// references being created until all objects have been read in.
// Essentially, we need a reference within the structure to itself,
// but the explicit lifetimes (necessary for the Elf library) prevent
// this. Thus, this is broken into two structures across two phases of
// processing, the first generating the ComposeSpec structure based on
// specification processing, and Compose which performs the binary
// processing and resulting computation.
//
// There are a number of dependencies involved in this process that
// determine the order in which the operations must be
// performed. These include:
//
// - Component ids. Component id assignment can only be done once we
// understand the DAG structure of the components. The component id
// formulation generates an initarg for compid for the booter.
//
//   Dependency: [ID assignment] -> [Parse specification, ascertain
//   DAG structure]
//
// - Initial Arguments. The initial arguments require parsing the
// specification for per-component arguments. Additionally, they
// require id assignment for "at" arguments.
//
// - Booter and resource manager parameters. The parameters for the
// booter requires that component ids have been formulated for each
// component being booted. Objects and specifications are indexed by
// id.
//
//   Dependency: [Booter + manager params] -> [Component ids for
//   dependencies, synchronous invocations formulated, id assignment]
//
// - Capability layouts. By default, booters have the extended
// capability table layout, and other components have bump-pointer
// allocated capability spans. The synchronous invocations are
// dynamically bound as the ucap structure (structure references on
// each synchronous invocation, including the capability id for that
// invocation) is populated by the booter. But other capabilities
// (e.g. resource tables and components) are early bound and
// referenced through the initial arguments.
//
//   Dependency: [Capability layout] -> [Parse the SInv sites, process
//   resource managers (this component and parents), id assignment]
//
// - Binary creation. The final binary with valid addresses requires
// that essentially all resources that need to be compiled into the
// binary are resolved. This includes all initargs, thus all resource
// manager and booter specifications.
//
//   Dependency: [binaries] -> [capability layouts, capability layout,
//   resource managers formulated, component ids, initargs
//   formulation]
//
// - Client synchronous invocations. Creating the client SInv
// information requires populating the ucap structure with the proper
// addresses. This is one of the latest bound pieces of information as
// it requires the client and server to be completely baked, and their
// capability layouts to be known in the arguments.
//
//   Dependency: [SInv creation] -> [Interface + variant resolution,
//   initargs parsing and creation, resource management ]
//
// - Booter synchronous invocations. The booter has to create the
// synchronous invocations for each component at bootup time. To do
// so, it must know the client ucap address, and the server function
// addresses. To populate the ucap structure, it must know the
// capability layout of the client.
//
//   Dependency: [booter SInvs] -> [binary layout (for addresses),
//   capability layout]
//
// Note that there are two inherent contradictions here:
//
// 1. the booter must know the server addresses to formulate the SInv
// information that gets packed into initargs in the booter. Thus, the
// addresses *in the booter* must be retrieved *before* it's binary is
// in the final form. We make the strong assumption that the code will
// not change between when we compile without the initargs, and after,
// thus the relevant addresses should remain stable. [TODO] add checks
// to validate this assumption.
//
// 2. We need the client to be binary-stable to get the information to
// populate the ucap, but updating the ucap requires modifying the
// binary. Instead of formulating this as a fixed point, convergence
// problem, we simply move the population of the ucap to runtime.
//
// - Manager parameters. The manager information requires
// understanding the relationships between components which requires
// understanding which interfaces are provided by which component, and
// parsing resource specifications. Mainly, it requires the capability
// layouts for the manager.
//
//   Dependency: [Manager parameters] -> [Capability layouts]
//
// Summary: We cannot actually build a component until we have
// relatively complete information about components that are dependent
// on it. This is especially so if the dependency encodes resource
// management, or booter responsibilities. Resource managers will need
// to know where the capabilities are that enable them to manage their
// children. Booters need to have the final addresses of their
// components to be able to layout the synchronous invocations,
// complete with addresses, in addition to the proper upcall
// addresses. Thus, we focus system construction on the following process:
//
// calculate component dependencies
// id assignment
// "at" initargs formulation [TODO]
// calculate resource manager and delegation relationships
// straightforward initargs formulation (including tarball)
// "resource"-centric initargs formulation (i.e. static creation of a message channel) [TODO]
// foreach component, sorted by dependency, most trusted first:
//   capability table allocations for the child resource manager information
//   formulate initargs for child resource manager information
//   formulate the initargs information for parent resource managers (to control delegation)
// foreach component, sorted by dependency, least trusted first:
//   normal component:
//     binary creation
//   booter component:
//     sinv address information (in client & server) retrieval and initargs formulation
//     tarball creation including all dependant components
//     binary creation
// at runtime: client sinv ucap population

use build::{comp_path, comps_base_path, interface_path, BuildContext};
use compobject::CompObject;
use cossystem::CosSystem;
use resources::{ComponentId, CoreNum, Res};
use passes::SInv;
use syshelpers::{dump_file, emit_file, exec_pipeline, reset_dir};

use std::collections::{BTreeMap, HashMap};
use std::env;

pub struct ComposeSpec {
    sysspec: String,
    src_path: String,
    sys: CosSystem,
    binaries: BTreeMap<String, Vec<u8>>,
    build_ctxt: BuildContext,
}

pub struct Compose {
    spec: ComposeSpec,
    comp_objs: BTreeMap<String, CompObject>,
    sinvs: Vec<SInv>,
    ids: BTreeMap<String, (String, ComponentId)>, // varname -> (imgname, index)
    mgrs: HashMap<ComponentId, Vec<Res>>,         // resources for a component
}

impl ComposeSpec {
    pub fn parse_spec(sysspec: String) -> Result<ComposeSpec, String> {
        let sys = CosSystem::parse(sysspec.clone())?;
        let mut bins = BTreeMap::new();
        let mut comps_path = comps_base_path();
        let mut build_ctxt = BuildContext::new(&sys.comps(), sys.booter());

        if let Err(s) = build_ctxt.validate_deps() {
            return Err(s);
        }
        build_ctxt.build_components();

        for c in sys.comps().iter() {
            let obj_path = build_ctxt.comp_obj_path(c.name()).unwrap();
            let obj_contents = dump_file(&obj_path)?;

            bins.insert(c.name().clone(), obj_contents);
        }

        Ok(ComposeSpec {
            sysspec: sysspec,
            src_path: comps_path,
            sys: sys,
            binaries: bins,
            build_ctxt: build_ctxt,
        })
    }

    pub fn sysspec_output(&self) -> &CosSystem {
        &self.sys
    }
}

impl Compose {
    pub fn parse_binaries(spec: ComposeSpec) -> Result<Compose, String> {
        let mut cs = BTreeMap::new();

        // build all of the component objects associated with the component binaries
        for c in spec.sys.comps().iter() {
            cs.insert(
                c.name().clone(),
                CompObject::parse(c.name().clone(), spec.binaries.get(c.name()).unwrap())?,
            );
        }

        // Find a total order of components based on the dependency
        // relation...
        let mut tot_ord = Vec::new();
        // which components are still remaining without an placement
        // in the total order?
        let mut remaining: BTreeMap<String, Vec<String>> = BTreeMap::new();
        for (n, _) in cs.iter() {
            remaining.insert(
                n.clone(),
                spec.build_ctxt
                    .comp_deps(n)
                    .unwrap()
                    .iter()
                    .map(|(d, _)| d.clone())
                    .collect(),
            );
        }
        let ncomps = remaining.len();
        loop {
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
            // Did we find a cycle?  TODO error out properly
            let len = no_deps.len();
            assert!(len > 0);
            for j in 0..len {
                remaining.remove(&no_deps[j]);
                tot_ord.push(no_deps[j].clone());
            }
            if remaining.len() == 0 {
                break;
            }
        }
        let mut ids = BTreeMap::new();
        let mut id = 1;
        for c in tot_ord {
            ids.insert(c.clone(), (spec.build_ctxt.comp_obj_name(&c).unwrap(), id));
            id = id + 1;
        }

        // We have completed all dependencies, and now have the
        // objects for symbol processing.  Time to create the sinv
        // dependencies.
        let mut sinvs = Vec::new();

        for (n, c) in cs.iter() {
            let cli = cs.get(n).unwrap();
            for (s, i) in spec.build_ctxt.comp_deps(n).unwrap().iter() {
                let srv = cs.get(s).unwrap();
                let mut cnt = 0;
                let mut unresolved = Vec::new();

                for dep in cli.dependencies().iter() {
                    let mut found = false;

                    for exp in srv.exported().iter() {
                        if dep.name() == exp.name() {
                            sinvs.push(SInv {
                                name: dep.name().clone(),
                                client: (*ids.get(n).unwrap()).1, // use the integer id here
                                server: (*ids.get(s).unwrap()).1, // ...and here
                                c_fn_addr: dep.func_addr(),
                                c_ucap_addr: dep.ucap_addr(),
                                s_fn_addr: exp.addr(),
                            });
                            cnt = cnt + 1;
                            found = true;
                            break;
                        }
                    }
                    if !found {
                        unresolved.push((
                            dep.name().clone(),
                            cli.name().clone(),
                            srv.name().clone(),
                        ));
                    }
                }
            }
        }

        let mut all = Compose {
            spec: spec,
            comp_objs: cs,
            sinvs: sinvs,
            ids: ids,
            mgrs: HashMap::new(),
        };
        all.spec.build_ctxt.gen_booter(&all);

        Ok(all)
    }

    pub fn components(&self) -> &BTreeMap<String, CompObject> {
        &self.comp_objs
    }

    pub fn sinvs(&self) -> &Vec<SInv> {
        &self.sinvs
    }

    pub fn ids(&self) -> &BTreeMap<String, (String, ComponentId)> {
        &self.ids
    }

    pub fn booter(&self) -> String {
        self.spec.sysspec_output().booter().clone()
    }
}
