use crate::{
    nodes::{Expression, Literal, StructEntry, Typename},
    scopes::{find_var, RootScope, Scope},
};

#[derive(Debug)]
pub enum Signedness {
    Signed,
    Unsigned,
    Unknonwn,
}

#[derive(Debug)]
pub struct Bytes {
    sign: Signedness,
    size: usize, // bits, 0 for unknown
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

pub fn infer_type(e: &Expression, rs: &RootScope, scopes: &Vec<Scope>) -> Result<Type, String> {
    match e {
        Expression::ArrayIndex { array, index: _ } => Ok(deref(infer_type(array, rs, scopes)?)?),
        Expression::BinaryOp { op: _, a, b: _ } => {
            // Not checking anything.
            let ta = infer_type(a, rs, scopes);
            // let tb = infer_type(b, rs, scopes)?;
            return ta;
        }
        Expression::FieldAccess {
            op: _,
            target,
            field_name,
        } => {
            let t = infer_type(target, rs, scopes)?;
            match t {
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
                _ => Err(String::from("field access on a non-struct")),
            }
        }
        Expression::Cast {
            type_name,
            operand: _,
        } => get_type(&type_name.type_name, rs),
        Expression::CompositeLiteral(x) => {
            if x.entries.len() == 0 {
                Ok(Type::Opaque)
            } else if x.entries[0].is_index {
                Ok(Type::Typeop {
                    target: Box::new(Type::Bytes(Bytes {
                        sign: Signedness::Unknonwn,
                        size: 1,
                    })),
                    hops: 1,
                })
            } else {
                Ok(Type::Struct {
                    opaque: false,
                    fields: Vec::new(),
                })
            }
        }
        Expression::FunctionCall {
            function,
            arguments: _,
        } => match infer_type(function, rs, scopes)? {
            Type::Opaque => Ok(Type::Opaque),
            Type::Func { rettype } => Ok(*rettype),
            _ => {
                panic!("not a func");
            }
        },
        Expression::Identifier(x) => {
            match find_var(scopes, &x.name) {
                Some(v) => {
                    return Ok(get_type(&v.typename, rs)?);
                }
                None => {}
            }
            match find_stdlib(&x.name) {
                Some(v) => {
                    return Ok(v);
                }
                None => {}
            }
            // dbg!(x, scopes);
            // panic!("var not found");
            return Err(String::from(format!("var {} not found", &x.name)));
        }
        Expression::Literal(x) => match x {
            Literal::Char(_) => Ok(Type::Bytes(Bytes {
                sign: Signedness::Unknonwn,
                size: 1,
            })),
            Literal::String(_) => Ok(addr(Type::Bytes(Bytes {
                sign: Signedness::Unknonwn,
                size: 1,
            }))),
            Literal::Number(_) => Ok(Type::Bytes(Bytes {
                sign: Signedness::Unknonwn,
                size: 0,
            })),
            Literal::Null => Ok(Type::Null),
        },
        Expression::NsName(x) => Ok(Type::Opaque),
        Expression::PostfixOperator { operator, operand } => match operator.as_str() {
            "++" | "--" => infer_type(operand, rs, scopes),
            _ => {
                dbg!(operator);
                todo!();
            }
        },
        Expression::PrefixOperator { operator, operand } => match operator.as_str() {
            "++" | "--" => infer_type(operand, rs, scopes),
            "!" | "-" => infer_type(operand, rs, scopes),
            "*" => Ok(deref(infer_type(operand, rs, scopes)?)?),
            "&" => Ok(addr(infer_type(operand, rs, scopes)?)),
            _ => {
                dbg!(operator);
                todo!();
            }
        },
        Expression::Sizeof { argument: _ } => Ok(Type::Bytes(Bytes {
            sign: Signedness::Signed,
            size: 0,
        })),
    }
}

fn find_stdlib(name: &String) -> Option<Type> {
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

fn addr(t: Type) -> Type {
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

fn deref(t: Type) -> Result<Type, String> {
    match t {
        Type::Typeop { target, hops } => {
            if hops == 1 {
                return Ok(*target);
            }
            return Ok(Type::Typeop {
                target,
                hops: hops - 1,
            });
        }
        _ => Err(String::from("dereference of a non-pointer")),
    }
}

fn get_type(tn: &Typename, r: &RootScope) -> Result<Type, String> {
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
            crate::nodes::ModuleObject::Typedef(_) => todo!(),
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
