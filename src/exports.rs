use crate::nodes::*;

#[derive(Debug)]
pub struct Exports {
    pub consts: Vec<EnumItem>,
    pub fns: Vec<FunctionDeclaration>,
    pub types: Vec<String>,
    // pub structs: Vec<StructTypedef>,
}

pub fn get_exports(m: &Module) -> Exports {
    let mut exports = Exports {
        consts: Vec::new(),
        fns: Vec::new(),
        types: Vec::new(),
        // structs: Vec::new(),
    };
    for e in &m.elements {
        match e {
            ModElem::Enum(x) => {
                if x.is_pub {
                    for m in &x.members {
                        exports.consts.push(m.clone());
                    }
                }
            }
            ModElem::Typedef(x) => {
                if x.is_pub {
                    exports.types.push(x.alias.name.clone());
                }
            }
            ModElem::StructTypedef(x) => {
                if x.is_pub {
                    exports.types.push(x.name.name.clone());
                }
            }
            // ModuleObject::StructAliasTypedef {
            //     is_pub,
            //     struct_name: _,
            //     type_alias,
            // } => {
            //     if *is_pub {
            //         exports.structs.push(StructTypedef {
            //             fields: Vec::new(),
            //             is_pub: *is_pub,
            //             name: type_alias.clone(),
            //         })
            //     }
            // }
            ModElem::FunctionDeclaration(f) => {
                if f.is_pub {
                    exports.fns.push(f.clone())
                }
            }
            _ => {}
        }
    }
    return exports;
}
