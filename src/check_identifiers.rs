use crate::{c, exports::Exports, nodes::*, parser::Error};
use std::collections::{HashMap, HashSet};

struct StrPos {
    str: String,
    pos: String,
}

pub fn run(m: &Module, exports: &HashMap<String, &Exports>) -> Vec<Error> {
    let mut scope = get_module_scope(m);
    let mut errors: Vec<Error> = Vec::new();
    let mut declared_local_types = Vec::new();
    let mut used_local_types = HashSet::new();

    for e in &m.elements {
        match e {
            ModuleObject::Typedef(x) => {
                if !x.is_pub {
                    declared_local_types.push(StrPos {
                        str: x.alias.name.clone(),
                        pos: String::from("?"),
                    });
                }
                check_ns_id(&x.type_name.name, &mut errors, &scope, exports);
            }
            ModuleObject::Enum { is_pub: _, members } => {
                for m in members {
                    if m.value.is_some() {
                        check_expr(m.value.as_ref().unwrap(), &mut errors, &scope, exports);
                    }
                }
            }
            ModuleObject::FunctionDeclaration(f) => {
                let tn = &f.type_name.name;
                if tn.namespace == "" {
                    used_local_types.insert(tn.name.clone());
                }
                check_ns_id(&f.type_name.name, &mut errors, &scope, exports);
                let mut function_scope = scope.clone();
                for pl in &f.parameters.list {
                    check_ns_id(&pl.type_name.name, &mut errors, &scope, exports);
                    for f in &pl.forms {
                        function_scope.push(&f.name);
                    }
                }
                check_body(
                    &f.body,
                    &mut errors,
                    &function_scope,
                    exports,
                    &mut used_local_types,
                );
            }
            ModuleObject::Macro { name, value } => {
                if name == "known" {
                    scope.push(value.trim());
                }
            }
            ModuleObject::ModuleVariable {
                type_name,
                form: _,
                value,
            } => {
                let n = &type_name.name;
                if n.namespace == "" {
                    used_local_types.insert(n.name.clone());
                }
                check_ns_id(&type_name.name, &mut errors, &scope, exports);
                check_expr(value, &mut errors, &scope, exports);
            }
            ModuleObject::StructAliasTypedef {
                is_pub,
                type_alias,
                struct_name: _,
            } => {
                if !is_pub {
                    declared_local_types.push(StrPos {
                        str: type_alias.clone(),
                        pos: String::from("?"),
                    });
                }
            }
            ModuleObject::StructTypedef(x) => {
                if !x.is_pub {
                    declared_local_types.push(StrPos {
                        str: x.name.name.clone(),
                        pos: String::from("?"),
                    });
                }
                for f in &x.fields {
                    match f {
                        StructEntry::Plain(x) => {
                            let n = &x.type_name.name;
                            if n.namespace == "" {
                                used_local_types.insert(n.name.clone());
                            }
                            check_ns_id(&x.type_name.name, &mut errors, &scope, exports);
                        }
                        StructEntry::Union(x) => {
                            for f in &x.fields {
                                let n = &f.type_name.name;
                                if n.namespace == "" {
                                    used_local_types.insert(n.name.clone());
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    for t in declared_local_types {
        if !used_local_types.contains(&t.str) {
            errors.push(Error {
                message: format!("unused type: {}", t.str),
                pos: t.pos.clone(),
            })
        }
    }
    return errors;
}

fn get_module_scope(m: &Module) -> Vec<&str> {
    let mut scope = vec![
        // should be fixed
        "break", "continue", "false", "NULL", "true", //
        // custom
        "nelem", "panic",
    ];
    for s in c::CCONST {
        scope.push(s);
    }
    for s in c::CFUNCS {
        scope.push(s);
    }
    for s in c::CTYPES {
        scope.push(s);
    }

    for e in &m.elements {
        match e {
            ModuleObject::FunctionDeclaration(f) => {
                scope.push(f.form.name.as_str());
            }
            ModuleObject::Enum { is_pub: _, members } => {
                for m in members {
                    scope.push(m.id.name.as_str());
                }
            }
            ModuleObject::Macro { name, value } => {
                if name == "define" {
                    let mut parts = value.trim().split(" ");
                    scope.push(parts.next().unwrap());
                }
                if name == "type" {
                    scope.push(value.trim());
                }
            }
            ModuleObject::ModuleVariable {
                type_name: _,
                form,
                value: _,
            } => {
                scope.push(form.name.as_str());
            }
            ModuleObject::StructTypedef(x) => {
                scope.push(x.name.name.as_str());
            }
            ModuleObject::Typedef(x) => {
                scope.push(x.alias.name.as_str());
            }
            ModuleObject::StructAliasTypedef {
                is_pub: _,
                struct_name: _,
                type_alias,
            } => {
                scope.push(type_alias.as_str());
            }
        }
    }
    return scope;
}

fn check_body(
    body: &Body,
    errors: &mut Vec<Error>,
    parent_scope: &Vec<&str>,
    exports: &HashMap<String, &Exports>,
    used_types: &mut HashSet<String>,
) {
    let mut scope: Vec<&str> = parent_scope.clone();
    for s in &body.statements {
        match s {
            Statement::Expression(e) => {
                check_expr(e, errors, &scope, exports);
            }
            Statement::Panic { arguments, pos: _ } => {
                for e in arguments {
                    check_expr(e, errors, &scope, exports);
                }
            }
            Statement::For {
                init,
                condition,
                action,
                body,
            } => {
                let loop_scope = &mut scope.clone();
                match init {
                    ForInit::LoopCounterDeclaration {
                        type_name,
                        form,
                        value,
                    } => {
                        check_ns_id(&type_name.name, errors, loop_scope, exports);
                        check_expr(value, errors, loop_scope, exports);
                        loop_scope.push(form.name.as_str());
                    }
                    ForInit::Expression(x) => {
                        check_expr(x, errors, loop_scope, exports);
                    }
                }
                check_expr(condition, errors, loop_scope, exports);
                check_expr(action, errors, loop_scope, exports);
                check_body(body, errors, loop_scope, exports, used_types);
            }
            Statement::If {
                condition,
                body,
                else_body,
            } => {
                check_expr(condition, errors, &scope, exports);
                check_body(body, errors, &scope, exports, used_types);
                if else_body.is_some() {
                    check_body(
                        else_body.as_ref().unwrap(),
                        errors,
                        &scope,
                        exports,
                        used_types,
                    );
                }
            }
            Statement::Return { expression } => match expression {
                Some(x) => {
                    check_expr(x, errors, &scope, exports);
                }
                None => {}
            },
            Statement::Switch {
                value,
                cases,
                default_case: default,
            } => {
                check_expr(value, errors, &scope, exports);
                for c in cases {
                    for v in &c.values {
                        match v {
                            SwitchCaseValue::Identifier(x) => {
                                check_ns_id(x, errors, &scope, exports);
                            }
                            SwitchCaseValue::Literal(_) => {}
                        }
                    }
                    check_body(&c.body, errors, &scope, exports, used_types);
                }
                if default.is_some() {
                    check_body(
                        default.as_ref().unwrap(),
                        errors,
                        &scope,
                        exports,
                        used_types,
                    );
                }
            }
            Statement::VariableDeclaration {
                type_name,
                forms,
                values,
            } => {
                if type_name.name.namespace == "" {
                    used_types.insert(type_name.name.name.clone());
                }
                check_ns_id(&type_name.name, errors, &scope, exports);
                for v in values {
                    if v.is_some() {
                        check_expr(v.as_ref().unwrap(), errors, &scope, exports);
                    }
                }
                for f in forms {
                    scope.push(f.name.as_str());
                }
            }
            Statement::While { condition, body } => {
                check_expr(condition, errors, &scope, exports);
                check_body(body, errors, &scope, exports, used_types);
            }
        }
    }
}

fn check_expr(
    e: &Expression,
    errors: &mut Vec<Error>,
    scope: &Vec<&str>,
    exports: &HashMap<String, &Exports>,
) {
    match e {
        Expression::ArrayIndex { array, index } => {
            check_expr(array, errors, scope, exports);
            check_expr(index, errors, scope, exports);
        }
        Expression::BinaryOp { op, a, b } => {
            if op != "->" && op != "." {
                check_expr(a, errors, scope, exports);
                check_expr(b, errors, scope, exports);
            }
        }
        Expression::Cast { type_name, operand } => {
            check_ns_id(&type_name.type_name.name, errors, scope, exports);
            check_expr(operand, errors, scope, exports);
        }
        Expression::CompositeLiteral(x) => {
            for e in &x.entries {
                check_expr(&e.value, errors, scope, exports);
            }
        }
        Expression::FunctionCall {
            function,
            arguments,
        } => {
            check_expr(function, errors, scope, exports);
            for x in arguments {
                check_expr(x, errors, scope, exports);
            }
        }
        Expression::Identifier(x) => {
            check_id(x, errors, scope);
        }
        Expression::Literal(_) => {}
        Expression::NsName(x) => {
            check_ns_id(x, errors, scope, exports);
        }
        Expression::PostfixOperator {
            operator: _,
            operand,
        } => {
            check_expr(operand, errors, scope, exports);
        }
        Expression::PrefixOperator {
            operator: _,
            operand,
        } => {
            check_expr(operand, errors, scope, exports);
        }
        Expression::Sizeof { argument } => {
            match argument.as_ref() {
                SizeofArgument::Typename(_) => {
                    //
                }
                SizeofArgument::Expression(x) => {
                    check_expr(&x, errors, scope, exports);
                }
            }
        }
    }
}

fn check_ns_id(
    x: &NsName,
    errors: &mut Vec<Error>,
    scope: &Vec<&str>,
    exports: &HashMap<String, &Exports>,
) {
    if x.namespace == "OS" {
        return;
    }
    if x.namespace != "" {
        let e = exports.get(&x.namespace).unwrap();
        for f in &e.fns {
            if f.form.name == x.name {
                return;
            }
        }
        if e.types.contains(&x.name) {
            return;
        }
        for c in &e.consts {
            if c.id.name == x.name {
                return;
            }
        }
        errors.push(Error {
            message: format!("{} doesn't have exported {}", x.namespace, x.name),
            pos: x.pos.clone(),
        });
    } else {
        if !scope.contains(&x.name.as_str()) {
            errors.push(Error {
                message: format!("unknown identifier: {}", x.name),
                pos: x.pos.clone(),
            });
        }
    }
}

fn check_id(x: &Identifier, errors: &mut Vec<Error>, scope: &Vec<&str>) {
    if !scope.contains(&x.name.as_str()) {
        errors.push(Error {
            message: format!("unknown identifier: {}", x.name),
            pos: x.pos.clone(),
        });
    }
}
