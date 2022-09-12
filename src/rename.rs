use crate::nodes::*;

pub fn rename_mod(m: &mut Module, prefix: &String, names: &Vec<String>) {
    for obj in &mut m.elements {
        mod_obj(obj, prefix, names);
    }
}

fn mod_obj(obj: &mut ModuleObject, prefix: &String, names: &Vec<String>) {
    match obj {
        ModuleObject::ModuleVariable(x) => {
            rename_typename(&mut x.type_name, prefix, names);
            expr(&mut x.value, prefix, names);
            rename_form(&mut x.form, prefix, names);
        }
        ModuleObject::Enum(Enum { is_pub, members }) => {
            if !*is_pub {
                return;
            }
            for mut m in members {
                m.id = rename(&m.id, prefix, names);
            }
        }
        ModuleObject::FunctionDeclaration(FunctionDeclaration {
            is_pub: _,
            type_name,
            form,
            parameters,
            body,
        }) => {
            rename_typename(type_name, prefix, names);
            params(parameters, prefix, names);
            rename_body(body, prefix, names);
            rename_form(form, prefix, names);
        }
        ModuleObject::Typedef(Typedef {
            type_name,
            form,
            is_pub: _,
        }) => {
            match &mut form.params {
                Some(x) => {
                    for tf in &mut x.forms {
                        rename_typename(&mut tf.type_name, prefix, names);
                    }
                }
                None => {}
            }
            form.alias = rename(&form.alias, prefix, names);
            rename_typename(type_name, prefix, names);
        }
        ModuleObject::StructTypedef(StructTypedef {
            fields,
            name: _,
            is_pub: _,
        }) => {
            // form.alias = rename(&form.alias, prefix, names);
            for e in fields {
                match e {
                    StructEntry::Plain(x) => {
                        rename_typename(&mut x.type_name, prefix, names);
                    }
                    StructEntry::Union(x) => {
                        for f in &mut x.fields {
                            rename_typename(&mut f.type_name, prefix, names);
                        }
                    }
                }
            }
        }
        _ => {
            //
        }
    }
}

fn params(p: &mut FunctionParameters, prefix: &String, names: &Vec<String>) {
    for p in &mut p.list {
        rename_typename(&mut p.type_name, prefix, names);
    }
}

fn rename_body(b: &mut Body, prefix: &String, names: &Vec<String>) {
    for s in &mut b.statements {
        rename_statement(s, prefix, names);
    }
}

fn rename_statement(s: &mut Statement, prefix: &String, names: &Vec<String>) {
    match s {
        Statement::VariableDeclaration {
            type_name,
            forms,
            values,
        } => {
            for f in forms {
                rename_form(f, prefix, names);
            }
            for v in values {
                match v {
                    Some(e) => expr(e, prefix, names),
                    None => {}
                }
            }
            rename_typename(type_name, prefix, names)
        }
        Statement::If {
            condition,
            body,
            else_body,
        } => {
            expr(condition, prefix, names);
            rename_body(body, prefix, names);
            match else_body {
                Some(x) => rename_body(x, prefix, names),
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
                    expr(e, prefix, names);
                }
                ForInit::LoopCounterDeclaration {
                    type_name,
                    form,
                    value,
                } => {
                    rename_typename(type_name, prefix, names);
                    rename_form(form, prefix, names);
                    expr(value, prefix, names);
                }
            }
            expr(condition, prefix, names);
            expr(action, prefix, names);
            rename_body(body, prefix, names);
        }
        Statement::While { condition, body } => {
            expr(condition, prefix, names);
            rename_body(body, prefix, names);
        }
        Statement::Return { expression } => match expression {
            Some(e) => {
                expr(e, prefix, names);
            }
            None => {}
        },
        Statement::Switch {
            value,
            cases,
            default,
        } => {
            expr(value, prefix, names);
            for c in cases {
                match &c.value {
                    SwitchCaseValue::Identifier(x) => {
                        c.value = SwitchCaseValue::Identifier(rename(x, prefix, names))
                    }
                    SwitchCaseValue::Literal(_) => {}
                }
                for s in &mut c.body.statements {
                    rename_statement(s, prefix, names);
                }
            }
            match default {
                Some(ss) => {
                    for s in &mut ss.statements {
                        rename_statement(s, prefix, names);
                    }
                }
                None => {}
            }
        }
        Statement::Expression(e) => {
            expr(e, prefix, names);
        }
    }
}

fn rename_form(f: &mut Form, prefix: &String, names: &Vec<String>) {
    f.name = rename(&f.name, prefix, names);
    for i in &mut f.indexes {
        match i {
            Some(e) => {
                expr(e, prefix, names);
            }
            None => {}
        }
    }
}

fn rename_typename(t: &mut Typename, prefix: &String, names: &Vec<String>) {
    t.name = rename(&t.name, prefix, names);
}

fn expr(e: &mut Expression, prefix: &String, names: &Vec<String>) {
    match e {
        Expression::Literal(_) => {}
        Expression::CompositeLiteral(x) => {
            for e in &mut x.entries {
                if e.key.is_some() {
                    let x = e.key.as_mut().unwrap();
                    expr(x, prefix, names);
                }
                expr(&mut e.value, prefix, names);
            }
        }
        Expression::Identifier(x) => *e = Expression::Identifier(rename(x, prefix, names)),
        Expression::BinaryOp { op: _, a, b } => {
            expr(a, prefix, names);
            expr(b, prefix, names);
        }
        Expression::PrefixOperator {
            operator: _,
            operand,
        } => {
            expr(operand, prefix, names);
        }
        Expression::PostfixOperator {
            operator: _,
            operand,
        } => {
            expr(operand, prefix, names);
        }
        Expression::Cast { type_name, operand } => {
            rename_typename(&mut type_name.type_name, prefix, names);
            expr(operand, prefix, names);
        }
        Expression::FunctionCall {
            function,
            arguments,
        } => {
            expr(function, prefix, names);
            for arg in arguments {
                expr(arg, prefix, names);
            }
        }
        Expression::Sizeof { argument } => match &mut **argument {
            SizeofArgument::Expression(e) => {
                expr(e, prefix, names);
            }
            SizeofArgument::Typename(e) => {
                rename_typename(e, prefix, names);
            }
        },
        Expression::ArrayIndex { array, index } => {
            expr(array, prefix, names);
            expr(index, prefix, names);
        }
    }
}

fn rename(current: &String, prefix: &String, names: &Vec<String>) -> String {
    return if names.contains(current) {
        format!("{}_{}", prefix, current)
    } else {
        current.clone()
    };
}
