use crate::exports::get_exports;
use crate::nodes::*;

// globalize a module: prefix each exported name with modid_
pub fn prefix_module(m: &mut Module, prefix: &String) {
    let exports = get_exports(&m);
    let mut names: Vec<String> = Vec::new();
    for e in exports.consts {
        names.push(e.id.name);
    }
    for f in exports.fns {
        names.push(f.form.name);
    }
    for t in exports.types {
        names.push(t);
    }
    for obj in &mut m.elements {
        prefix_mod_obj(obj, prefix, &names);
    }
}

fn prefix_mod_obj(obj: &mut ModuleObject, prefix: &String, names: &Vec<String>) {
    match obj {
        ModuleObject::ModuleVariable {
            form,
            type_name,
            value,
        } => {
            prefix_typename(type_name, prefix, names);
            prefix_expr(value, prefix, names);
            prefix_form(form, prefix, names);
        }
        ModuleObject::Enum { is_pub, members } => {
            if !*is_pub {
                return;
            }
            for mut m in members {
                m.id.name = newname(&m.id.name, prefix, names);
            }
        }
        ModuleObject::FunctionDeclaration(FunctionDeclaration {
            is_pub: _,
            type_name,
            form,
            parameters,
            body,
        }) => {
            prefix_typename(type_name, prefix, names);
            for p in &mut parameters.list {
                prefix_typename(&mut p.type_name, prefix, names);
            }
            prefix_body(body, prefix, names);
            prefix_form(form, prefix, names);
        }
        ModuleObject::Typedef(Typedef {
            pos: _,
            type_name,
            is_pub: _,
            alias,
            array_size: _,
            dereference_count: _,
            function_parameters,
        }) => {
            match function_parameters {
                Some(x) => {
                    for tf in &mut x.forms {
                        prefix_typename(&mut tf.type_name, prefix, names);
                    }
                }
                None => {}
            }
            alias.name = newname(&alias.name, prefix, names);
            prefix_typename(type_name, prefix, names);
        }
        ModuleObject::StructTypedef(StructTypedef {
            pos: _,
            fields,
            name,
            is_pub: _,
        }) => {
            name.name = newname(&name.name, prefix, names);
            for e in fields {
                match e {
                    StructEntry::Plain(x) => {
                        prefix_typename(&mut x.type_name, prefix, names);
                    }
                    StructEntry::Union(x) => {
                        for f in &mut x.fields {
                            prefix_typename(&mut f.type_name, prefix, names);
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

fn prefix_body(b: &mut Body, prefix: &String, names: &Vec<String>) {
    for s in &mut b.statements {
        prefix_statement(s, prefix, names);
    }
}

fn prefix_statement(s: &mut Statement, prefix: &String, names: &Vec<String>) {
    match s {
        Statement::VariableDeclaration {
            type_name,
            forms,
            values,
        } => {
            for f in forms {
                prefix_form(f, prefix, names);
            }
            for v in values {
                match v {
                    Some(e) => prefix_expr(e, prefix, names),
                    None => {}
                }
            }
            prefix_typename(type_name, prefix, names)
        }
        Statement::If {
            condition,
            body,
            else_body,
        } => {
            prefix_expr(condition, prefix, names);
            prefix_body(body, prefix, names);
            match else_body {
                Some(x) => prefix_body(x, prefix, names),
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
                    prefix_expr(e, prefix, names);
                }
                ForInit::LoopCounterDeclaration {
                    type_name,
                    form,
                    value,
                } => {
                    prefix_typename(type_name, prefix, names);
                    prefix_form(form, prefix, names);
                    prefix_expr(value, prefix, names);
                }
            }
            prefix_expr(condition, prefix, names);
            prefix_expr(action, prefix, names);
            prefix_body(body, prefix, names);
        }
        Statement::While { condition, body } => {
            prefix_expr(condition, prefix, names);
            prefix_body(body, prefix, names);
        }
        Statement::Return { expression } => match expression {
            Some(e) => {
                prefix_expr(e, prefix, names);
            }
            None => {}
        },
        Statement::Switch {
            value,
            cases,
            default_case: default,
        } => {
            prefix_expr(value, prefix, names);
            for c in cases.iter_mut() {
                for v in c.values.iter_mut() {
                    match v {
                        SwitchCaseValue::Identifier(x) => {
                            x.name = newname(&x.name, prefix, names);
                        }
                        SwitchCaseValue::Literal(_) => {}
                    }
                }
                for s in &mut c.body.statements {
                    prefix_statement(s, prefix, names);
                }
            }
            match default {
                Some(ss) => {
                    for s in &mut ss.statements {
                        prefix_statement(s, prefix, names);
                    }
                }
                None => {}
            }
        }
        Statement::Panic { arguments, pos: _ } => {
            for e in arguments {
                prefix_expr(e, prefix, names);
            }
        }
        Statement::Expression(e) => {
            prefix_expr(e, prefix, names);
        }
    }
}

fn prefix_form(f: &mut Form, prefix: &String, names: &Vec<String>) {
    f.name = newname(&f.name, prefix, names);
    for i in &mut f.indexes {
        match i {
            Some(e) => {
                prefix_expr(e, prefix, names);
            }
            None => {}
        }
    }
}

fn prefix_typename(t: &mut Typename, prefix: &String, names: &Vec<String>) {
    if t.name.namespace == "" {
        t.name.name = newname(&t.name.name, prefix, names);
    }
}

fn prefix_expr(e: &mut Expression, prefix: &String, names: &Vec<String>) {
    match e {
        Expression::NsName(n) => {
            if n.namespace == "" {
                n.name = newname(&n.name, prefix, names);
            }
        }
        Expression::Literal(_) => {}
        Expression::CompositeLiteral(x) => {
            for e in &mut x.entries {
                if e.key.is_some() {
                    let x = e.key.as_mut().unwrap();
                    prefix_expr(x, prefix, names);
                }
                prefix_expr(&mut e.value, prefix, names);
            }
        }
        Expression::Identifier(x) => {
            x.name = newname(&x.name, prefix, names);
        }
        Expression::BinaryOp { op: _, a, b } => {
            prefix_expr(a, prefix, names);
            prefix_expr(b, prefix, names);
        }
        Expression::PrefixOperator {
            operator: _,
            operand,
        } => {
            prefix_expr(operand, prefix, names);
        }
        Expression::PostfixOperator {
            operator: _,
            operand,
        } => {
            prefix_expr(operand, prefix, names);
        }
        Expression::Cast { type_name, operand } => {
            prefix_typename(&mut type_name.type_name, prefix, names);
            prefix_expr(operand, prefix, names);
        }
        Expression::FunctionCall {
            function,
            arguments,
        } => {
            prefix_expr(function, prefix, names);
            for arg in arguments {
                prefix_expr(arg, prefix, names);
            }
        }
        Expression::Sizeof { argument } => match &mut **argument {
            SizeofArgument::Expression(e) => {
                prefix_expr(e, prefix, names);
            }
            SizeofArgument::Typename(e) => {
                prefix_typename(e, prefix, names);
            }
        },
        Expression::ArrayIndex { array, index } => {
            prefix_expr(array, prefix, names);
            prefix_expr(index, prefix, names);
        }
    }
}

fn newname(current: &String, prefix: &String, names: &Vec<String>) -> String {
    return if names.contains(current) {
        format!("{}__{}", prefix, current)
    } else {
        current.clone()
    };
}
