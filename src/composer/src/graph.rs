use passes::{
    component, BuildState, ComponentId, ComponentName, Dependency, GraphPass, Interface,
    ServiceType, SystemState, Transition, Variant,
};
use petgraph::dot::Dot;
use petgraph::graph::{DefaultIx, NodeIndex};
use petgraph::stable_graph::StableGraph;
use petgraph::visit::NodeRef;
use std::collections::HashMap;
use std::fmt;
use syshelpers::{emit_file, exec_pipeline};

#[derive(Clone, Eq, PartialEq, Debug, Hash)]
struct ComponentNode {
    id: ComponentId,
    name: ComponentName,
    properties: Vec<ServiceType>,
}

#[derive(Clone, Eq, PartialEq, Debug, Hash)]
struct InterfaceNode {
    name: Interface,
    variant: Variant,
    server: ComponentName, // to ensure that Eq and Hash work properly
}

impl From<&Dependency> for InterfaceNode {
    fn from(dep: &Dependency) -> Self {
        InterfaceNode {
            name: dep.interface.clone(),
            variant: dep.variant.clone(),
            server: dep.server.clone(),
        }
    }
}

#[derive(Clone, Eq, PartialEq, Debug, Hash)]
enum GraphNode {
    Comp(ComponentNode),
    Interface(InterfaceNode),
}

#[derive(Clone, Eq, PartialEq, Debug, Hash)]
enum GraphEdge {
    CompDep,
    IfDep,
    IfExp,
}

impl fmt::Display for ComponentNode {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "{} ({}){}",
            self.name,
            self.id,
            self.properties
                .iter()
                .fold("\n".to_string(), |s, p| format!(
                    "{}{}",
                    s,
                    match p {
                        ServiceType::Scheduler => "Scheduler\n".to_string(),
                        ServiceType::CapMgr => "Capability Manager\n".to_string(),
                        ServiceType::Constructor => "Constructor\n".to_string(),
                    }
                ))
        )
    }
}

impl fmt::Display for InterfaceNode {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{} (variant: {})", self.name, self.variant,)
    }
}

impl fmt::Display for GraphNode {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // recursively call the displays of the constituent types
        write!(
            f,
            "{}",
            match self {
                GraphNode::Comp(c) => c.to_string(),
                GraphNode::Interface(i) => i.to_string(),
            }
        )
    }
}

impl fmt::Display for GraphEdge {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "")
    }
}

pub struct Graph {
    graph: StableGraph<GraphNode, GraphEdge>,
}

#[derive(PartialEq, Eq)]
enum GraphOutput {
    Interfaces,
}

impl Graph {
    fn build(s: &SystemState) -> Self {
        let mut g = StableGraph::new();
        let comps: Vec<_> = s
            .get_named()
            .ids()
            .iter()
            .map(|(id, _)| component(s, id))
            .collect();
        let mut g_comp_map: HashMap<ComponentName, NodeIndex<DefaultIx>> = HashMap::new();
        let mut g_if_map: HashMap<InterfaceNode, NodeIndex<DefaultIx>> = HashMap::new();

        for c in &comps {
            let id = *s
                .get_named()
                .rmap()
                .get(&c.name)
                .expect("Didn't find a component in the named rmap that must be in it!");
            // Create a graph node for a component
            let gid = g.add_node(GraphNode::Comp(ComponentNode {
                name: c.name.clone(),
                id,
                properties: Vec::new(), // TODO: add in the properties
            }));
            // ...and make sure that we can look up the node successful to create edges.
            g_comp_map.insert(c.name.clone(), gid);

            // Lets make the interface nodes.
            for dep in s.get_spec().deps_named(&c.name) {
                let ifnode = InterfaceNode::from(dep);
                // If we haven't yet added the interface, throw it in!
                if g_if_map.get(&ifnode).is_none() || dep.variant == "kernel" {
                    let g_if_id = g.add_node(GraphNode::Interface(ifnode.clone()));
                    g_if_map.insert(ifnode, g_if_id);
                }
            }
        }
        // Now that we have nodes for all of the components, lets throw
        // together the edges.
        for c in &comps {
            let err = "Graph construction: somehow the component is looked up without being added.";
            let from_id = g_comp_map.get(&c.name).expect(err);

            for dep in s.get_spec().deps_named(&c.name) {
                if dep.server == ComponentName::new(&"kernel".to_string(), &"global".to_string()) {
                    continue;
                }
                let to_id = g_comp_map.get(&dep.server).expect(err);
                let if_id = g_if_map.get(&InterfaceNode::from(dep)).expect(err);

                if !g.contains_edge(*from_id, *to_id) {
                    // Not saving the edge id as we can look it up with `find_edge`
                    g.add_edge(*from_id, *to_id, GraphEdge::CompDep);
                }
                if !g.contains_edge(*from_id, *if_id) {
                    g.add_edge(*from_id, *if_id, GraphEdge::IfDep);
                }
                if !g.contains_edge(*if_id, *to_id) {
                    g.add_edge(*if_id, *to_id, GraphEdge::IfExp);
                }
            }
        }

        Graph { graph: g }
    }

    fn render(&self, out: &[GraphOutput]) -> String {
        let output_interfaces = out
            .iter()
            .find(|&opt| *opt == GraphOutput::Interfaces)
            .is_some();
        format!(
            "{}",
            Dot::with_attr_getters(
                &self.graph.filter_map(
                    |_, n| {
                        match n {
                            n @ &GraphNode::Comp(_) => Some(n),
                            n @ &GraphNode::Interface(_) if output_interfaces => Some(n),
                            _ => None,
                        }
                    },
                    |_, e| {
                        match e {
                            &GraphEdge::CompDep if output_interfaces => None,
                            _ => Some(e),
                        }
                    },
                ),
                &[],
                &|_, _| "".to_string(),
                &|_, n| match n.weight() {
                    GraphNode::Comp(_) => "style=filled fillcolor=\"skyblue\"".to_string(),
                    GraphNode::Interface(_) => "shape=rect".to_string(),
                }
            )
        )
    }
}

impl Transition for Graph {
    fn transition(c: &SystemState, b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let g = Graph::build(c);
        let dotpath_comp = b.file_path(&"component_graph.dot".to_string())?;
        let dotpath_if = b.file_path(&"interfaces_graph.dot".to_string())?;

        if let Err(s) = emit_file(&dotpath_comp, g.render(&[]).as_bytes()) {
            return Err(s);
        }
        if let Err(s) = emit_file(
            &dotpath_if,
            g.render(&[GraphOutput::Interfaces]).as_bytes(),
        ) {
            return Err(s);
        }

        let comp_cmd = format!("dot -Tpdf -O {}", dotpath_comp);
        let _ = exec_pipeline(vec![comp_cmd]);
        let if_cmd = format!("dot -Tpdf -O {}", dotpath_if);
        let _ = exec_pipeline(vec![if_cmd]);

        Ok(Box::new(g))
    }
}

impl GraphPass for Graph {}
