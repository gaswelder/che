use crate::nodes::*;

// globalize a module: prefix each exported name with modid_
pub fn prefix_module(m: &mut Module, prefix: &String) {
    let exports = &m.exports;
    let mut exported_names: Vec<String> = Vec::new();
    for e in &exports.consts {
        exported_names.push(e.id.name.clone());
    }
    for f in &exports.fns {
        exported_names.push(f.form.name.clone());
    }
    for t in &exports.types {
        exported_names.push(t.clone());
    }
    for obj in &mut m.elements {
        prefix_mod_obj(obj, prefix, &exported_names);
    }
}

fn prefix_mod_obj(obj: &mut ModElem, prefix: &String, names: &Vec<String>) {
    match obj {
        ModElem::ModuleVariable(x) => {
            prefix_typename(&mut x.type_name, prefix, names);
            match &mut x.value {
                Some(v) => {
                    prefix_expr(v, prefix, names);
                }
                None => {}
            }
            prefix_form(&mut x.form, prefix, names);
        }
        ModElem::Enum(x) => {
            if !x.is_pub {
                return;
            }
            for m in &mut x.entries {
                m.id.name = newname(&m.id.name, prefix, names);
            }
        }
        ModElem::DeclFunc(DeclFunc {
            is_pub: _,
            type_name,
            form,
            parameters,
            body,
            pos: _,
        }) => {
            prefix_typename(type_name, prefix, names);
            for p in &mut parameters.list {
                prefix_typename(&mut p.type_name, prefix, names);
            }
            prefix_body(body, prefix, names);
            prefix_form(form, prefix, names);
        }
        ModElem::Typedef(Typedef {
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
        ModElem::StructTypedef(StructTypedef {
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
        Statement::Break => {}
        Statement::Continue => {}
        Statement::VariableDeclaration(x) => {
            prefix_form(&mut x.form, prefix, names);
            match &mut x.value {
                Some(e) => prefix_expr(e, prefix, names),
                None => {}
            }
            prefix_typename(&mut x.type_name, prefix, names)
        }
        Statement::If(x) => {
            let condition = &mut x.condition;
            let body = &mut x.body;
            let else_body = &mut x.else_body;
            prefix_expr(condition, prefix, names);
            prefix_body(body, prefix, names);
            match else_body {
                Some(x) => prefix_body(x, prefix, names),
                None => {}
            }
        }
        Statement::For(x) => {
            let init = &mut x.init;
            let condition = &mut x.condition;
            let action = &mut x.action;
            let body = &mut x.body;
            match init {
                Some(init) => match init {
                    ForInit::Expr(e) => {
                        prefix_expr(e, prefix, names);
                    }
                    ForInit::DeclLoopCounter {
                        type_name,
                        form,
                        value,
                    } => {
                        prefix_typename(type_name, prefix, names);
                        prefix_form(form, prefix, names);
                        prefix_expr(value, prefix, names);
                    }
                },
                None => {}
            }
            match condition {
                Some(condition) => {
                    prefix_expr(condition, prefix, names);
                }
                None => {}
            }
            match action {
                Some(action) => {
                    prefix_expr(action, prefix, names);
                }
                None => {}
            }
            prefix_body(body, prefix, names);
        }
        Statement::While(x) => {
            let condition = &mut x.cond;
            let body = &mut x.body;
            prefix_expr(condition, prefix, names);
            prefix_body(body, prefix, names);
        }
        Statement::Return(x) => {
            let expression = &mut x.expression;
            match expression {
                Some(e) => {
                    prefix_expr(e, prefix, names);
                }
                None => {}
            }
        }
        Statement::Switch(x) => {
            let value = &mut x.value;
            let cases = &mut x.cases;
            let default = &mut x.default_case;
            prefix_expr(value, prefix, names);
            for c in cases.iter_mut() {
                for v in c.values.iter_mut() {
                    match v {
                        SwitchCaseValue::Ident(x) => {
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
        Expression::BinaryOp(x) => {
            let a = &mut x.a;
            let b = &mut x.b;
            prefix_expr(a, prefix, names);
            prefix_expr(b, prefix, names);
        }
        Expression::FieldAccess(x) => {
            let target = &mut x.target;
            prefix_expr(target, prefix, names);
        }
        Expression::PrefixOperator(x) => {
            let operand = &mut x.operand;
            prefix_expr(operand, prefix, names);
        }
        Expression::PostfixOperator(x) => {
            let operand = &mut x.operand;
            prefix_expr(operand, prefix, names);
        }
        Expression::Cast(x) => {
            let type_name = &mut x.type_name;
            let operand = &mut x.operand;
            prefix_typename(&mut type_name.type_name, prefix, names);
            prefix_expr(operand, prefix, names);
        }
        Expression::FunctionCall(x) => {
            prefix_expr(&mut x.func, prefix, names);
            for arg in &mut x.args {
                prefix_expr(arg, prefix, names);
            }
        }
        Expression::Sizeof(x) => {
            let argument = &mut x.argument;
            match &mut **argument {
                SizeofArg::Expr(e) => {
                    prefix_expr(e, prefix, names);
                }
                SizeofArg::Typename(e) => {
                    prefix_typename(e, prefix, names);
                }
            }
        }
        Expression::ArrayIndex(x) => {
            prefix_expr(&mut x.array, prefix, names);
            prefix_expr(&mut x.index, prefix, names);
        }
    }
}

fn newname(current: &String, prefix: &String, exported_names: &Vec<String>) -> String {
    return if exported_names.contains(current) {
        format!("{}__{}", prefix, current)
    } else {
        current.clone()
    };
}
