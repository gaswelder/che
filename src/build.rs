use crate::nodes;

pub struct Import {
    pub path: String,
}

// Returns a list of import nodes from the module.
pub fn module_imports(module: &nodes::Module) -> Vec<Import> {
    let mut list: Vec<Import> = vec![];
    for element in &module.elements {
        match element {
            nodes::ModuleObject::Import { path } => list.push(Import { path: path.clone() }),
            _ => {}
        }
    }
    return list;
}
