use std::collections::HashMap;

use crate::{
    buf::Pos,
    cspec,
    nodes::{Form, FunctionDeclaration, ImportNode, Module, ModuleObject, Typename},
    resolve::getns,
};

#[derive(Clone, Debug)]
pub struct Scope {
    pub vars: HashMap<String, ScopeItem1<VarInfo>>,
}

pub fn newscope() -> Scope {
    return Scope {
        vars: HashMap::new(),
    };
}

#[derive(Clone, Debug)]
pub struct RootScope {
    pub pre: Vec<String>,
    pub imports: HashMap<String, ScopeItem1<ImportNode>>,
    pub types: HashMap<String, ScopeItem1<ModuleObject>>,
    pub consts: HashMap<String, ScopeItem>,
    pub vars: HashMap<String, ScopeItem1<VarInfo>>,
    pub funcs: HashMap<String, ScopeItem1<FunctionDeclaration>>,
}

#[derive(Clone, Debug)]
pub struct ScopeItem {
    pub read: bool,
    pub pos: Pos,
    pub ispub: bool,
}

#[derive(Clone, Debug)]
pub struct ScopeItem1<T> {
    pub read: bool,
    pub val: T,
}

#[derive(Clone, Debug)]
pub struct VarInfo {
    pub pos: Pos,
    pub typename: Typename,
    pub form: Form,
}

pub fn get_module_scope(m: &Module) -> RootScope {
    let mut s = RootScope {
        pre: vec![
            String::from("false"),
            String::from("NULL"),
            String::from("true"),
            // custom
            String::from("nelem"),
            String::from("panic"),
            String::from("min"),
            String::from("max"),
        ],
        imports: HashMap::new(),
        consts: HashMap::new(),
        vars: HashMap::new(),
        funcs: HashMap::new(),
        types: HashMap::new(),
    };
    // let mut scope = vec![
    //     // should be fixed
    //     "break", "continue", "false", "NULL", "true", //
    //     // custom
    //     "nelem", "panic", "min", "max",
    // ];
    for c in cspec::CCONST {
        s.pre.push(c.to_string());
    }
    for c in cspec::CFUNCS {
        s.pre.push(c.to_string());
    }
    for c in cspec::CTYPES {
        s.pre.push(c.to_string());
    }

    for e in &m.elements {
        match e {
            ModuleObject::Import(x) => {
                let ns = getns(&x.specified_path);
                s.imports.insert(
                    ns,
                    ScopeItem1 {
                        read: false,
                        val: x.clone(),
                    },
                );
            }
            ModuleObject::FunctionDeclaration(f) => {
                s.funcs.insert(
                    f.form.name.clone(),
                    ScopeItem1 {
                        read: false,
                        val: f.clone(),
                    },
                );
            }
            ModuleObject::Enum {
                is_pub,
                members,
                pos: _,
            } => {
                for m in members {
                    s.consts.insert(
                        m.id.name.clone(),
                        ScopeItem {
                            read: false,
                            pos: m.pos.clone(),
                            ispub: *is_pub,
                        },
                    );
                }
            }
            ModuleObject::Macro { name, value, pos } => {
                if name == "define" {
                    let mut parts = value.trim().split(" ");
                    s.consts.insert(
                        String::from(parts.next().unwrap()),
                        ScopeItem {
                            read: false,
                            pos: pos.clone(),
                            ispub: false,
                        },
                    );
                }
                if name == "type" {
                    s.pre.push(String::from(value.trim()));
                }
            }
            ModuleObject::ModuleVariable(x) => {
                s.vars.insert(
                    x.form.name.clone(),
                    ScopeItem1 {
                        read: false,
                        val: VarInfo {
                            pos: x.pos.clone(),
                            typename: x.type_name.clone(),
                            form: x.form.clone(),
                            // ispub: false,
                        },
                    },
                );
            }
            ModuleObject::StructTypedef(x) => {
                s.types.insert(
                    x.name.name.clone(),
                    ScopeItem1 {
                        read: false,
                        val: e.clone(),
                    },
                );
            }
            ModuleObject::Typedef(x) => {
                s.types.insert(
                    x.alias.name.clone(),
                    ScopeItem1 {
                        read: false,
                        val: e.clone(),
                    },
                );
            }
            ModuleObject::StructAliasTypedef {
                pos: _,
                is_pub: _,
                struct_name: _,
                type_alias,
            } => {
                s.types.insert(
                    type_alias.clone(),
                    ScopeItem1 {
                        read: false,
                        val: e.clone(),
                    },
                );
            }
        }
    }
    return s;
}

pub fn find_var(scopes: &Vec<Scope>, name: &String) -> Option<VarInfo> {
    let n = scopes.len();
    for i in 0..n {
        let s = &scopes[n - i - 1];
        match s.vars.get(name) {
            Some(v) => {
                return Some(v.val.clone());
            }
            None => {}
        }
    }
    return None;
}
