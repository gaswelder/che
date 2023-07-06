use crate::{c, exports::Exports, nodes::*, parser::Error, resolve::getns};
use std::collections::{HashMap, HashSet};

struct StrPos {
    str: String,
    pos: String,
}

struct State {
    errors: Vec<Error>,
    used_namespaces: HashSet<String>,
}

pub fn run(m: &Module, imports: &HashMap<String, &Exports>) -> Vec<Error> {
    let mut scope = get_module_scope(m);
    let mut declared_local_types = Vec::new();
    let mut used_local_types = HashSet::new();

    let mut import_nodes = Vec::new();

    let mut state = State {
        errors: Vec::new(),
        used_namespaces: HashSet::new(),
    };

    for e in &m.elements {
        match e {
            ModuleObject::Import(x) => import_nodes.push(x),
            ModuleObject::Typedef(x) => {
                if !x.is_pub {
                    declared_local_types.push(StrPos {
                        str: x.alias.name.clone(),
                        pos: x.pos.clone(),
                    });
                }
                state
                    .used_namespaces
                    .insert(x.type_name.name.namespace.clone());
                check_ns_id(&x.type_name.name, &mut state, &scope, imports);
            }
            ModuleObject::Enum { is_pub: _, members } => {
                for m in members {
                    match &m.value {
                        Some(v) => check_expr(v, &mut state, &scope, imports),
                        None => {}
                    }
                }
            }
            ModuleObject::FunctionDeclaration(f) => {
                let tn = &f.type_name.name;
                if tn.namespace == "" {
                    used_local_types.insert(tn.name.clone());
                }
                check_ns_id(&f.type_name.name, &mut state, &scope, imports);
                let mut function_scope = scope.clone();
                for pl in &f.parameters.list {
                    check_ns_id(&pl.type_name.name, &mut state, &scope, imports);
                    for f in &pl.forms {
                        function_scope.push(&f.name);
                    }
                }
                check_body(
                    &f.body,
                    &mut state,
                    &function_scope,
                    imports,
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
                check_ns_id(&type_name.name, &mut state, &scope, imports);
                check_expr(value, &mut state, &scope, imports);
            }
            ModuleObject::StructAliasTypedef {
                pos,
                is_pub,
                type_alias,
                struct_name: _,
            } => {
                if !is_pub {
                    declared_local_types.push(StrPos {
                        str: type_alias.clone(),
                        pos: pos.clone(),
                    });
                }
            }
            ModuleObject::StructTypedef(x) => {
                if !x.is_pub {
                    declared_local_types.push(StrPos {
                        str: x.name.name.clone(),
                        pos: x.pos.clone(),
                    });
                }
                for f in &x.fields {
                    match f {
                        StructEntry::Plain(x) => {
                            let n = &x.type_name.name;
                            if n.namespace == "" {
                                used_local_types.insert(n.name.clone());
                            }
                            check_ns_id(&x.type_name.name, &mut state, &scope, imports);
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
            state.errors.push(Error {
                message: format!("unused type: {}", t.str),
                pos: t.pos.clone(),
            })
        }
    }
    for node in import_nodes {
        let ns = getns(&node.specified_path);
        if !state.used_namespaces.contains(&ns) {
            state.errors.push(Error {
                message: format!("unused import: {}", node.specified_path),
                pos: node.pos.clone(),
            })
        }
    }

    return state.errors;
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
            ModuleObject::Import(_) => {}
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
                pos: _,
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
    state: &mut State,
    parent_scope: &Vec<&str>,
    imports: &HashMap<String, &Exports>,
    used_types: &mut HashSet<String>,
) {
    let mut scope: Vec<&str> = parent_scope.clone();
    for s in &body.statements {
        match s {
            Statement::Expression(e) => {
                check_expr(e, state, &scope, imports);
            }
            Statement::Panic { arguments, pos: _ } => {
                for e in arguments {
                    check_expr(e, state, &scope, imports);
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
                        check_ns_id(&type_name.name, state, loop_scope, imports);
                        check_expr(value, state, loop_scope, imports);
                        loop_scope.push(form.name.as_str());
                    }
                    ForInit::Expression(x) => {
                        check_expr(x, state, loop_scope, imports);
                    }
                }
                check_expr(condition, state, loop_scope, imports);
                check_expr(action, state, loop_scope, imports);
                check_body(body, state, loop_scope, imports, used_types);
            }
            Statement::If {
                condition,
                body,
                else_body,
            } => {
                check_expr(condition, state, &scope, imports);
                check_body(body, state, &scope, imports, used_types);
                if else_body.is_some() {
                    check_body(
                        else_body.as_ref().unwrap(),
                        state,
                        &scope,
                        imports,
                        used_types,
                    );
                }
            }
            Statement::Return { expression } => match expression {
                Some(x) => {
                    check_expr(x, state, &scope, imports);
                }
                None => {}
            },
            Statement::Switch {
                value,
                cases,
                default_case: default,
            } => {
                check_expr(value, state, &scope, imports);
                for c in cases {
                    for v in &c.values {
                        match v {
                            SwitchCaseValue::Identifier(x) => {
                                check_ns_id(x, state, &scope, imports);
                            }
                            SwitchCaseValue::Literal(_) => {}
                        }
                    }
                    check_body(&c.body, state, &scope, imports, used_types);
                }
                if default.is_some() {
                    check_body(
                        default.as_ref().unwrap(),
                        state,
                        &scope,
                        imports,
                        used_types,
                    );
                }
            }
            Statement::VariableDeclaration {
                type_name,
                form,
                value,
                pos: _,
            } => {
                if type_name.name.namespace == "" {
                    used_types.insert(type_name.name.name.clone());
                }
                check_ns_id(&type_name.name, state, &scope, imports);
                if value.is_some() {
                    check_expr(value.as_ref().unwrap(), state, &scope, imports);
                }
                scope.push(form.name.as_str());
            }
            Statement::While { condition, body } => {
                check_expr(condition, state, &scope, imports);
                check_body(body, state, &scope, imports, used_types);
            }
        }
    }
}

fn check_expr(
    e: &Expression,
    state: &mut State,
    scope: &Vec<&str>,
    imports: &HashMap<String, &Exports>,
) {
    match e {
        Expression::ArrayIndex { array, index } => {
            check_expr(array, state, scope, imports);
            check_expr(index, state, scope, imports);
        }
        Expression::BinaryOp { op: _, a, b } => {
            check_expr(a, state, scope, imports);
            check_expr(b, state, scope, imports);
        }
        Expression::FieldAccess {
            op: _,
            target,
            field_name: _,
        } => {
            check_expr(target, state, scope, imports);
        }
        Expression::Cast { type_name, operand } => {
            check_ns_id(&type_name.type_name.name, state, scope, imports);
            check_expr(operand, state, scope, imports);
        }
        Expression::CompositeLiteral(x) => {
            for e in &x.entries {
                check_expr(&e.value, state, scope, imports);
            }
        }
        Expression::FunctionCall {
            function,
            arguments,
        } => {
            check_expr(function, state, scope, imports);
            for x in arguments {
                check_expr(x, state, scope, imports);
            }
        }
        Expression::Identifier(x) => {
            check_id(x, state, scope);
        }
        Expression::Literal(_) => {}
        Expression::NsName(x) => {
            check_ns_id(x, state, scope, imports);
        }
        Expression::PostfixOperator {
            operator: _,
            operand,
        } => {
            check_expr(operand, state, scope, imports);
        }
        Expression::PrefixOperator {
            operator: _,
            operand,
        } => {
            check_expr(operand, state, scope, imports);
        }
        Expression::Sizeof { argument } => {
            match argument.as_ref() {
                SizeofArgument::Typename(_) => {
                    //
                }
                SizeofArgument::Expression(x) => {
                    check_expr(&x, state, scope, imports);
                }
            }
        }
    }
}

fn check_ns_id(
    x: &NsName,
    state: &mut State,
    scope: &Vec<&str>,
    imports: &HashMap<String, &Exports>,
) {
    if x.namespace == "OS" {
        return;
    }
    if x.namespace != "" {
        state.used_namespaces.insert(x.namespace.clone());
        let e = imports.get(&x.namespace).unwrap();
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
        state.errors.push(Error {
            message: format!("{} doesn't have exported {}", x.namespace, x.name),
            pos: x.pos.clone(),
        });
    } else {
        if !scope.contains(&x.name.as_str()) {
            state.errors.push(Error {
                message: format!("unknown identifier: {}", x.name),
                pos: x.pos.clone(),
            });
        }
    }
}

fn check_id(x: &Identifier, state: &mut State, scope: &Vec<&str>) {
    if !scope.contains(&x.name.as_str()) {
        state.errors.push(Error {
            message: format!("unknown identifier: {}", x.name),
            pos: x.pos.clone(),
        });
    }
}
