use crate::{
    c,
    nodes::{Form, Identifier, StructEntry, Typename},
    scopes::RootScope,
};

#[derive(Debug)]
pub enum Signedness {
    Signed,
    Unsigned,
    Unknonwn,
}

#[derive(Debug)]
pub struct Bytes {
    pub sign: Signedness,
    pub size: usize, // bits, 0 for unknown
}

#[derive(Debug)]
pub enum Type {
    Unknown,
    Null,
    Bool,
    Float,
    Double,
    Void,
    Voidp,
    Bytes(Bytes),
    Struct {
        opaque: bool,
        fields: Vec<StructEntry>,
    },
    Pointer {
        target: Box<Type>,
    },
    Func {
        rettype: Box<Type>,
    },
}

impl Type {
    pub fn fmt(&self) -> String {
        match self {
            Type::Unknown => format!("unknown"),
            Type::Null => format!("NULL"),
            Type::Bool => format!("bool"),
            Type::Float => format!("float"),
            Type::Double => format!("double"),
            Type::Void => format!("void"),
            Type::Voidp => format!("void*"),
            Type::Bytes(x) => match x.sign {
                Signedness::Signed => {
                    if x.size > 0 {
                        format!("{}-bit signed", x.size)
                    } else {
                        format!("x-bit signed")
                    }
                }
                Signedness::Unsigned => {
                    if x.size > 0 {
                        format!("{}-bit", x.size)
                    } else {
                        format!("x-bit unsigned")
                    }
                }
                Signedness::Unknonwn => {
                    if x.size == 8 {
                        format!("char")
                    } else if x.size > 0 {
                        format!("{}-bit value", x.size)
                    } else {
                        format!("bits of unknown size")
                    }
                }
            },
            Type::Struct { opaque, fields } => {
                if *opaque {
                    format!("opaque struct")
                } else {
                    let mut s = String::from("");
                    for f in fields {
                        match f {
                            StructEntry::Plain(x) => {
                                for f in &x.forms {
                                    s += &format!("{}, ", f.name);
                                }
                            }
                            StructEntry::Union(_) => {
                                s += &format!("{{union}}, ");
                            }
                        }
                    }
                    format!("struct {{ {}}}", s)
                }
            }
            Type::Pointer { target } => format!("pointer to {}", target.fmt()),
            Type::Func { rettype } => format!("function returning {}", rettype.fmt()),
        }
    }
}

pub fn sum(t1: Type, _t2: Type) -> Result<Type, String> {
    // Not checking anything for now.
    return Ok(t1);
}

pub fn access(
    op: &String,
    t: Type,
    field_name: &Identifier,
    rs: &RootScope,
) -> Result<Type, String> {
    match op.as_str() {
        "." => match t {
            Type::Unknown => Ok(t),
            Type::Struct { opaque: _, fields } => {
                for x in fields {
                    match x {
                        StructEntry::Plain(x) => {
                            for f in x.forms {
                                if f.name == field_name.name {
                                    return get_type(&x.type_name, rs);
                                }
                            }
                        }
                        StructEntry::Union(_) => return Ok(Type::Unknown),
                    }
                }
                return Err(String::from(format!(
                    "unknown struct field: {}",
                    &field_name.name
                )));
            }
            _ => Err(String::from(format!("field '.' access on a {}", t.fmt()))),
        },
        "->" => access(&String::from("."), deref(t)?, field_name, rs),
        _ => {
            dbg!(op);
            todo!();
        }
    }
}

pub fn find_stdlib(name: &String) -> Option<Type> {
    let tint = Box::new(Type::Bytes(Bytes {
        sign: Signedness::Signed,
        size: 0,
    }));
    match name.as_str() {
        "printf" | "strcmp" => Some(Type::Func { rettype: tint }),
        "calloc" => Some(Type::Func {
            rettype: Box::new(Type::Voidp),
        }),
        "NULL" => Some(Type::Null),
        _ => None,
    }
}

pub fn addr(t: Type) -> Type {
    Type::Pointer {
        target: Box::new(t),
    }
}

pub fn deref(t: Type) -> Result<Type, String> {
    match t {
        Type::Unknown => Ok(t),
        Type::Pointer { target } => {
            return Ok(*target);
        }
        _ => Err(format!("dereference of a non-pointer ({})", t.fmt())),
    }
}

pub fn get_type(tn: &Typename, r: &RootScope) -> Result<Type, String> {
    if tn.name.namespace == "" {
        match c::get_type(&tn.name.name) {
            Some(x) => return Ok(x.t),
            None => {}
        }
        match find_local_typedef(&tn, r) {
            Some(t) => Ok(t),
            None => return Err(format!("couldn't determine type of {}", &tn.name.name)),
        }
    } else {
        Ok(Type::Unknown)
    }
}

fn find_local_typedef(tn: &Typename, s: &RootScope) -> Option<Type> {
    match s.types.get(&tn.name.name) {
        Some(t) => match &t.val {
            crate::nodes::ModuleObject::StructAliasTypedef {
                pos: _,
                is_pub: _,
                struct_name: _,
                type_alias: _,
            } => Some(Type::Struct {
                opaque: true,
                fields: Vec::new(),
            }),
            crate::nodes::ModuleObject::Typedef(x) => {
                let mut result = match get_type(&x.type_name, s) {
                    Ok(t) => t,
                    Err(err) => panic!("{}", err),
                };
                let mut hops = x.dereference_count;
                if x.array_size > 0 {
                    hops += 1;
                }
                for _ in 0..hops {
                    result = addr(result);
                }
                match x.function_parameters {
                    Some(_) => Some(Type::Func {
                        rettype: Box::new(result),
                    }),
                    None => Some(result),
                }
            }
            crate::nodes::ModuleObject::StructTypedef(x) => Some(Type::Struct {
                opaque: false,
                fields: x.fields.clone(),
            }),
            _ => {
                panic!("unexpected object in scope types");
            }
        },
        None => None,
    }
}

pub fn derive_typeform(
    typename: &Typename,
    form: &Form,
    root_scope: &RootScope,
) -> Result<Type, String> {
    let t = get_type(typename, root_scope)?;
    let hops = form.hops + form.indexes.len();
    let mut r = t;
    for _ in 0..hops {
        r = addr(r);
    }
    return Ok(r);
}
