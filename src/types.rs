use crate::{
    nodes::{Identifier, StructEntry, Typename},
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
    Opaque,
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
    Typeop {
        target: Box<Type>,
        hops: usize,
    },
    Func {
        rettype: Box<Type>,
    },
}

impl Type {
    pub fn fmt(&self) -> String {
        match self {
            Type::Opaque => format!("unknown"),
            Type::Null => format!("NULL"),
            Type::Bool => format!("bool"),
            Type::Float => format!("float"),
            Type::Double => format!("double"),
            Type::Void => format!("void"),
            Type::Voidp => format!("void*"),
            Type::Bytes(_) => format!("{{bytes}}"),
            Type::Struct { opaque, fields } => format!("{{struct}}"),
            Type::Typeop { target, hops } => format!("pointer to {{...}}"),
            Type::Func { rettype } => format!("function returning {{...}}"),
        }
    }
}

pub fn sum(t1: Type, t2: Type) -> Result<Type, String> {
    // Not checking anything.
    return Ok(t1);
}

pub fn access(t: Type, field_name: &Identifier, rs: &RootScope) -> Result<Type, String> {
    match t {
        Type::Opaque => Ok(t),
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
                    StructEntry::Union(_) => return Ok(Type::Opaque),
                }
            }
            return Err(String::from(format!(
                "unknown struct field: {}",
                &field_name.name
            )));
        }
        _ => Err(String::from(format!("field access on a {}", t.fmt()))),
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
    match t {
        Type::Typeop { target, hops } => Type::Typeop {
            target,
            hops: hops + 1,
        },
        _ => Type::Typeop {
            target: Box::new(t),
            hops: 1,
        },
    }
}

pub fn deref(t: Type) -> Result<Type, String> {
    match t {
        Type::Opaque => Ok(t),
        Type::Typeop { target, hops } => {
            if hops == 1 {
                return Ok(*target);
            }
            return Ok(Type::Typeop {
                target,
                hops: hops - 1,
            });
        }
        _ => Err(format!("dereference of a non-pointer {}", t.fmt())),
    }
}

pub fn get_type(tn: &Typename, r: &RootScope) -> Result<Type, String> {
    if tn.name.namespace == "" {
        match get_builtin_type(&tn) {
            Some(x) => return Ok(x),
            None => {}
        }
        match find_local_typedef(&tn, r) {
            Some(t) => Ok(t),
            None => return Err(format!("couldn't determine type of {}", &tn.name.name)),
        }
    } else {
        Ok(Type::Opaque)
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
            crate::nodes::ModuleObject::Typedef(x) => None,
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

fn get_builtin_type(tn: &Typename) -> Option<Type> {
    match tn.name.name.as_str() {
        "bool" => Some(Type::Bool),
        "float" => Some(Type::Float),
        "double" => Some(Type::Double),
        "void" => Some(Type::Void),
        "char" => Some(Type::Bytes(Bytes {
            sign: Signedness::Unknonwn,
            size: 8,
        })),
        "clock_t" => Some(Type::Bytes(Bytes {
            sign: Signedness::Unsigned,
            size: 0,
        })),
        "int" => Some(Type::Bytes(Bytes {
            sign: Signedness::Signed,
            size: 0,
        })),
        "int16_t" => Some(Type::Bytes(Bytes {
            sign: Signedness::Signed,
            size: 16,
        })),
        "int32_t" => Some(Type::Bytes(Bytes {
            sign: Signedness::Signed,
            size: 32,
        })),
        "int64_t" => Some(Type::Bytes(Bytes {
            sign: Signedness::Signed,
            size: 64,
        })),
        "int8_t" => Some(Type::Bytes(Bytes {
            sign: Signedness::Signed,
            size: 8,
        })),
        "long" => Some(Type::Bytes(Bytes {
            sign: Signedness::Signed,
            size: 0,
        })),
        "ptrdiff_t" => Some(Type::Bytes(Bytes {
            sign: Signedness::Signed,
            size: 0,
        })),
        "short" => Some(Type::Bytes(Bytes {
            sign: Signedness::Signed,
            size: 0,
        })),
        "size_t" => Some(Type::Bytes(Bytes {
            sign: Signedness::Unsigned,
            size: 0,
        })),
        "time_t" => Some(Type::Bytes(Bytes {
            sign: Signedness::Unknonwn,
            size: 0,
        })),
        "uint32_t" => Some(Type::Bytes(Bytes {
            sign: Signedness::Unsigned,
            size: 32,
        })),
        "uint64_t" => Some(Type::Bytes(Bytes {
            sign: Signedness::Unsigned,
            size: 64,
        })),
        "unsigned" => Some(Type::Bytes(Bytes {
            sign: Signedness::Unsigned,
            size: 0,
        })),
        "jmp_buf" => Some(Type::Struct {
            opaque: true,
            fields: Vec::new(),
        }),
        "va_list" => Some(Type::Struct {
            opaque: true,
            fields: Vec::new(),
        }),
        "wchar_t" => Some(Type::Struct {
            opaque: true,
            fields: Vec::new(),
        }),
        "FILE" => Some(Type::Struct {
            opaque: true,
            fields: Vec::new(),
        }),
        _ => None,
    }
}
