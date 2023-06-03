use crate::nodes;
use crate::parser;

pub struct Import {
    pub path: String,
}

// Returns a list of import nodes from the module.
pub fn module_imports(m: &nodes::Module) -> Vec<Import> {
    let mut list: Vec<Import> = vec![];
    for element in &m.elements {
        match element {
            nodes::ModuleObject::Import { path } => list.push(Import { path: path.clone() }),
            _ => {}
        }
    }
    return list;
}

// Returns a list of all modules required to build the given module, including
// the same module.
pub fn resolve_deps(m: &nodes::Module) -> Vec<nodes::Module> {
    let mut deps: Vec<nodes::Module> = vec![m.clone()];
    let mut present = vec![m.id.clone()];
    for imp in module_imports(&m) {
        let sub = parser::get_module(&imp.path, &m.source_path).unwrap();
        for dep in resolve_deps(&sub) {
            if present.contains(&&dep.id) {
                continue;
            }
            present.push(dep.id.clone());
            deps.push(dep);
        }
    }
    return deps;
}
