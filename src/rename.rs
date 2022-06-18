use crate::nodes::*;

pub fn rename_mod(m: &mut Module) {
    for obj in &mut m.elements {
        mod_obj(obj);
    }
}

fn mod_obj(obj: &mut ModuleObject) {
    match obj {
        ModuleObject::ModuleVariable(x) => {
            rename_typename(&mut x.type_name);
            expr(&mut x.value);
            rename_form(&mut x.form);
        }
        ModuleObject::Enum(Enum { is_pub, members }) => {
            if !*is_pub {
                return;
            }
            for mut m in members {
                m.id = format!("{}_{}", "kekekekeke", m.id);
            }
        }
        ModuleObject::FunctionDeclaration(FunctionDeclaration {
            is_pub: _,
            type_name,
            form,
            parameters,
            body,
        }) => {
            rename_typename(type_name);
            params(parameters);
            rename_body(body);
            rename_form(form);
        }
        ModuleObject::Typedef(Typedef {
            type_name,
            form,
            is_pub: _,
        }) => {
            match &mut form.params {
                Some(x) => {
                    for tf in &mut x.forms {
                        rename_typename(&mut tf.type_name);
                    }
                }
                None => {}
            }
            form.alias = format!("{}_{}", "kekekeke", form.alias);

            match type_name {
                TypedefTarget::AnonymousStruct { entries } => {
                    for e in entries {
                        match e {
                            StructEntry::Plain(x) => {
                                rename_typename(&mut x.type_name);
                            }
                            StructEntry::Union(x) => {
                                for f in &mut x.fields {
                                    rename_typename(&mut f.type_name);
                                }
                            }
                        }
                    }
                }
                TypedefTarget::Typename(x) => {
                    rename_typename(x);
                }
            }
        }
        _ => {
            //
        }
    }
}

fn params(p: &mut FunctionParameters) {
    for p in &mut p.list {
        rename_typename(&mut p.type_name);
    }
}

fn rename_body(b: &mut Body) {
    for s in &mut b.statements {
        rename_statement(s);
    }
}

fn rename_statement(s: &mut Statement) {
    match s {
        Statement::VariableDeclaration {
            type_name,
            forms,
            values,
        } => {
            for f in forms {
                rename_form(f);
            }
            for v in values {
                match v {
                    Some(e) => expr(e),
                    None => {}
                }
            }
            rename_typename(type_name)
        }
        Statement::If {
            condition,
            body,
            else_body,
        } => {
            expr(condition);
            rename_body(body);
            match else_body {
                Some(x) => rename_body(x),
                None => {}
            }
        }
        Statement::For {
            init,
            condition,
            action,
            body,
        } => {
            match init {
                ForInit::Expression(e) => {
                    expr(e);
                }
                ForInit::LoopCounterDeclaration {
                    type_name,
                    form,
                    value,
                } => {
                    rename_typename(type_name);
                    rename_form(form);
                    expr(value);
                }
            }
            expr(condition);
            expr(action);
            rename_body(body);
        }
        Statement::While { condition, body } => {
            expr(condition);
            rename_body(body);
        }
        Statement::Return { expression } => match expression {
            Some(e) => {
                expr(e);
            }
            None => {}
        },
        Statement::Switch {
            value,
            cases,
            default,
        } => {
            expr(value);
            for c in cases {
                match &c.value {
                    SwitchCaseValue::Identifier(x) => {
                        c.value = SwitchCaseValue::Identifier(format!("{}_{}", "kekekeke", x))
                    }
                    SwitchCaseValue::Literal(_) => {}
                }
                for s in &mut c.statements {
                    rename_statement(s);
                }
            }
            match default {
                Some(ss) => {
                    for s in ss {
                        rename_statement(s);
                    }
                }
                None => {}
            }
        }
        Statement::Expression(e) => {
            expr(e);
        }
    }
}

fn rename_form(f: &mut Form) {
    f.name = format!("{}_{}", "kekekeke", f.name);
    for i in &mut f.indexes {
        match i {
            Some(e) => {
                expr(e);
            }
            None => {}
        }
    }
}

fn rename_typename(t: &mut Typename) {
    t.name = format!("{}_{}", "kekekeke", t.name);
}

fn expr(e: &mut Expression) {
    match e {
        Expression::Literal(x) => {}
        Expression::StructLiteral { members } => {
            for m in members {
                expr(&mut m.value);
            }
        }
        Expression::ArrayLiteral(x) => {
            for v in &mut x.values {
                match &v.index {
                    ArrayLiteralKey::Identifier(s) => {
                        v.index = ArrayLiteralKey::Identifier(format!("{}_{}", "kekekeke", s))
                    }
                    ArrayLiteralKey::None => {}
                    ArrayLiteralKey::Literal(_) => {}
                }
                match &v.value {
                    ArrayLiteralValue::Identifier(s) => {
                        v.value = ArrayLiteralValue::Identifier(format!("{}_{}", "kekekeke", s))
                    }
                    ArrayLiteralValue::ArrayLiteral(_) => {}
                    ArrayLiteralValue::Literal(_) => {}
                }
            }
        }
        Expression::Identifier(x) => *e = Expression::Identifier(format!("{}_{}", "kekekeke", x)),
        Expression::BinaryOp { op: _, a, b } => {
            expr(a);
            expr(b);
        }
        Expression::PrefixOperator {
            operator: _,
            operand,
        } => {
            expr(operand);
        }
        Expression::PostfixOperator {
            operator: _,
            operand,
        } => {
            expr(operand);
        }
        Expression::Cast { type_name, operand } => {
            rename_typename(&mut type_name.type_name);
            expr(operand);
        }
        Expression::FunctionCall {
            function,
            arguments,
        } => {
            println!("func call before: {:?}", function);
            expr(function);
            println!("func call after: {:?}", function);
            for arg in arguments {
                expr(arg);
            }
        }
        Expression::Sizeof { argument } => match &mut **argument {
            SizeofArgument::Expression(e) => {
                expr(e);
            }
            SizeofArgument::Typename(e) => {
                rename_typename(e);
            }
        },
        Expression::ArrayIndex { array, index } => {
            expr(array);
            expr(index);
        }
    }
}
