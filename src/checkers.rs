use crate::{
    buf::Pos,
    exports::Exports,
    node_queries::{body_returns, has_function_call, isvoid},
    nodes::*,
    parser::Error,
    scopes::{find_var, get_module_scope, newscope, RootScope, Scope, ScopeItem1, VarInfo},
};
use std::collections::HashMap;

struct State {
    errors: Vec<Error>,
    root_scope: RootScope,
}

pub fn run(m: &Module, imports: &HashMap<String, &Exports>) -> Vec<Error> {
    let mut state = State {
        errors: Vec::new(),
        root_scope: get_module_scope(m),
    };
    let mut scopestack: Vec<Scope> = Vec::new();

    for e in &m.elements {
        check_module_object(e, &mut state, &mut scopestack, imports);
    }

    for (k, v) in state.root_scope.imports {
        if !v.read {
            state.errors.push(Error {
                message: format!("unused import: {}", k),
                pos: v.val.pos.clone(),
            })
        }
    }
    for (k, v) in state.root_scope.types {
        if !v.ispub && !v.read {
            state.errors.push(Error {
                message: format!("unused type: {}", k),
                pos: v.pos.clone(),
            })
        }
    }
    for (k, v) in state.root_scope.consts {
        if !v.ispub && !v.read {
            state.errors.push(Error {
                message: format!("unused constant: {}", k),
                pos: v.pos,
            });
        }
    }
    for (k, v) in state.root_scope.funcs {
        if !v.val.is_pub && !v.read && k != "main" {
            state.errors.push(Error {
                message: format!("unused function: {}", k),
                pos: v.val.pos,
            });
        }
    }

    return state.errors;
}

fn check_module_object(
    e: &ModuleObject,
    state: &mut State,
    scopestack: &mut Vec<Scope>,
    imports: &HashMap<String, &Exports>,
) {
    match e {
        ModuleObject::Import(_) => {}
        ModuleObject::Typedef(x) => {
            check_ns_id(&x.type_name.name, state, scopestack, imports);
        }
        ModuleObject::Enum {
            is_pub: _,
            members,
            pos: _,
        } => {
            for m in members {
                match &m.value {
                    Some(v) => check_expr(v, state, scopestack, imports),
                    None => {}
                }
            }
        }
        ModuleObject::FunctionDeclaration(f) => {
            check_function_declaration(f, state, scopestack, imports);
        }
        ModuleObject::Macro { .. } => {}
        ModuleObject::ModuleVariable(x) => {
            // Local type? Register the usage.
            let n = &x.type_name.name;
            if n.namespace == "" {
                match state.root_scope.types.get_mut(&n.name) {
                    Some(t) => {
                        t.read = true;
                    }
                    None => {}
                }
            }

            // Array declaration? Look into the index expressions.
            for i in &x.form.indexes {
                match i {
                    Some(e) => check_expr(e, state, scopestack, imports),
                    None => {}
                }
            }

            if has_function_call(x.value.as_ref().unwrap()) {
                state.errors.push(Error {
                    message: format!("function call in module variable initialization"),
                    pos: x.pos.clone(),
                })
            }
            check_ns_id(&x.type_name.name, state, scopestack, imports);
            check_expr(x.value.as_ref().unwrap(), state, scopestack, imports);
        }
        ModuleObject::StructAliasTypedef { .. } => {}
        ModuleObject::StructTypedef(x) => {
            for f in &x.fields {
                match f {
                    StructEntry::Plain(x) => {
                        let n = &x.type_name.name;
                        if n.namespace == "" {
                            match state.root_scope.types.get_mut(&n.name) {
                                Some(t) => {
                                    t.read = true;
                                }
                                None => {}
                            }
                        }
                        check_ns_id(&x.type_name.name, state, scopestack, imports);
                        for f in &x.forms {
                            for i in &f.indexes {
                                match i {
                                    Some(e) => {
                                        check_expr(e, state, scopestack, imports);
                                    }
                                    None => {}
                                }
                            }
                        }
                    }
                    StructEntry::Union(x) => {
                        for f in &x.fields {
                            let n = &f.type_name.name;
                            if n.namespace == "" {
                                match state.root_scope.types.get_mut(&n.name) {
                                    Some(t) => {
                                        t.read = true;
                                    }
                                    None => {}
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

fn check_function_declaration(
    f: &FunctionDeclaration,
    state: &mut State,
    scopestack: &mut Vec<Scope>,
    imports: &HashMap<String, &Exports>,
) {
    let tn = &f.type_name.name;
    if tn.namespace == "" {
        match state.root_scope.types.get_mut(&tn.name) {
            Some(t) => {
                t.read = true;
            }
            None => {}
        }
    }

    check_ns_id(&f.type_name.name, state, scopestack, imports);

    scopestack.push(newscope());

    for tf in &f.parameters.list {
        if isvoid(&tf.type_name) {
            for f in &tf.forms {
                if f.stars == "" {
                    state.errors.push(Error {
                        message: format!("can't use void as an argument type"),
                        pos: tf.pos.clone(),
                    })
                }
            }
        }
        check_ns_id(&tf.type_name.name, state, scopestack, imports);
        for f in &tf.forms {
            let n = scopestack.len();
            scopestack[n - 1].vars.insert(
                f.name.clone(),
                ScopeItem1 {
                    read: false,
                    val: VarInfo {
                        pos: f.pos.clone(),
                        typename: tf.type_name.clone(),
                        form: f.clone(),
                    },
                },
            );
        }
    }
    check_body(&f.body, state, scopestack, imports);
    let s = scopestack.pop().unwrap();
    for (k, v) in s.vars {
        if !v.read {
            state.errors.push(Error {
                message: format!("{} not used", k),
                pos: v.val.pos,
            })
        }
    }

    if (!isvoid(&f.type_name) || f.form.stars != "") && !body_returns(&f.body) {
        state.errors.push(Error {
            message: format!("{}: missing return", f.form.name),
            pos: f.pos.clone(),
        })
    }
}

fn check_body(
    body: &Body,
    state: &mut State,
    scopestack: &mut Vec<Scope>,
    imports: &HashMap<String, &Exports>,
) {
    scopestack.push(newscope());
    for s in &body.statements {
        match s {
            Statement::Break => {}
            Statement::Continue => {}
            Statement::Expression(e) => {
                check_expr(e, state, scopestack, imports);
            }
            Statement::Panic { arguments, pos: _ } => {
                for e in arguments {
                    check_expr(e, state, scopestack, imports);
                }
            }
            Statement::For {
                init,
                condition,
                action,
                body,
            } => {
                scopestack.push(newscope());
                let n = scopestack.len();
                match init {
                    Some(init) => match init {
                        ForInit::LoopCounterDeclaration {
                            type_name,
                            form,
                            value,
                        } => {
                            check_ns_id(&type_name.name, state, scopestack, imports);
                            check_expr(value, state, scopestack, imports);
                            scopestack[n - 1].vars.insert(
                                form.name.clone(),
                                ScopeItem1 {
                                    read: true,
                                    val: VarInfo {
                                        pos: form.pos.clone(),
                                        typename: type_name.clone(),
                                        form: form.clone(),
                                        // ispub: false,
                                    },
                                },
                            );
                        }
                        ForInit::Expression(x) => {
                            check_expr(x, state, scopestack, imports);
                        }
                    },
                    None => {}
                }
                match condition {
                    Some(condition) => {
                        check_expr(condition, state, scopestack, imports);
                    }
                    None => {}
                }
                match action {
                    Some(action) => {
                        check_expr(action, state, scopestack, imports);
                    }
                    None => {}
                }
                check_body(body, state, scopestack, imports);
                scopestack.pop();
            }
            Statement::If {
                condition,
                body,
                else_body,
            } => {
                check_expr(condition, state, scopestack, imports);
                check_body(body, state, scopestack, imports);
                if else_body.is_some() {
                    check_body(else_body.as_ref().unwrap(), state, scopestack, imports);
                }
            }
            Statement::Return { expression } => match expression {
                Some(x) => {
                    check_expr(x, state, scopestack, imports);
                }
                None => {}
            },
            Statement::Switch {
                value,
                cases,
                default_case: default,
            } => {
                check_expr(value, state, scopestack, imports);
                for c in cases {
                    for v in &c.values {
                        match v {
                            SwitchCaseValue::Identifier(x) => {
                                check_ns_id(x, state, scopestack, imports);
                            }
                            SwitchCaseValue::Literal(_) => {}
                        }
                    }
                    check_body(&c.body, state, scopestack, imports);
                }
                if default.is_some() {
                    check_body(default.as_ref().unwrap(), state, scopestack, imports);
                }
            }
            Statement::VariableDeclaration(x) => {
                // Local type? Count as usage.
                let n = &x.type_name.name;
                if n.namespace == "" {
                    match state.root_scope.types.get_mut(&n.name) {
                        Some(t) => {
                            t.read = true;
                        }
                        None => {}
                    }
                }

                // If it's a variable declaration,
                // look into the index expressions.
                for i in &x.form.indexes {
                    match i {
                        Some(e) => {
                            check_expr(e, state, scopestack, imports);
                        }
                        None => {}
                    }
                }

                check_ns_id(&x.type_name.name, state, scopestack, imports);
                let n = scopestack.len();
                if x.value.is_some() {
                    scopestack[n - 1].vars.insert(
                        x.form.name.clone(),
                        ScopeItem1 {
                            read: false,
                            val: VarInfo {
                                pos: x.pos.clone(),
                                typename: x.type_name.clone(),
                                form: x.form.clone(), // ispub: false,
                            },
                        },
                    );
                    check_expr(x.value.as_ref().unwrap(), state, scopestack, imports);
                } else {
                    scopestack[n - 1].vars.insert(
                        x.form.name.clone(),
                        ScopeItem1 {
                            read: false,
                            val: VarInfo {
                                pos: x.pos.clone(),
                                typename: x.type_name.clone(),
                                form: x.form.clone(), // ispub: false,
                            },
                        },
                    );
                }
            }
            Statement::While { condition, body } => {
                check_expr(condition, state, scopestack, imports);
                check_body(body, state, scopestack, imports);
            }
        }
    }
    let s = scopestack.pop().unwrap();
    for (k, v) in s.vars {
        if !v.read {
            state.errors.push(Error {
                message: format!("unused variable: {}", k),
                pos: v.val.pos,
            });
        }
    }
}

fn check_expr(
    e: &Expression,
    state: &mut State,
    scopes: &mut Vec<Scope>,
    imports: &HashMap<String, &Exports>,
) {
    match e {
        Expression::ArrayIndex { array, index } => {
            check_expr(array, state, scopes, imports);
            check_expr(index, state, scopes, imports);
        }
        Expression::BinaryOp { op, a, b } => {
            if op == ">" {
                infer_type(a, scopes);
                infer_type(b, scopes);
            }
            check_expr(a, state, scopes, imports);
            check_expr(b, state, scopes, imports);
        }
        Expression::FieldAccess {
            op: _,
            target,
            field_name: _,
        } => {
            check_expr(target, state, scopes, imports);
        }
        Expression::Cast { type_name, operand } => {
            check_ns_id(&type_name.type_name.name, state, scopes, imports);
            check_expr(operand, state, scopes, imports);
        }
        Expression::CompositeLiteral(x) => {
            for e in &x.entries {
                check_expr(&e.value, state, scopes, imports);
            }
        }
        Expression::FunctionCall {
            function,
            arguments,
        } => {
            match function.as_ref() {
                Expression::Identifier(x) => match state.root_scope.funcs.get_mut(&x.name) {
                    Some(e) => {
                        e.read = true;
                        let f = &e.val;
                        let mut n = 0;
                        for p in &f.parameters.list {
                            n += p.forms.len();
                        }
                        let nactual = arguments.len();
                        if f.parameters.variadic {
                            if nactual < n {
                                state.errors.push(Error {
                                    message: format!(
                                        "{} expects at least {} arguments, got {}",
                                        f.form.name, n, nactual
                                    ),
                                    pos: x.pos.clone(),
                                })
                            }
                        } else {
                            if nactual != n {
                                state.errors.push(Error {
                                    message: format!(
                                        "{} expects {} arguments, got {}",
                                        f.form.name, n, nactual
                                    ),
                                    pos: x.pos.clone(),
                                })
                            }
                        }
                    }
                    None => {}
                },
                _ => {}
            }
            check_expr(function, state, scopes, imports);
            for x in arguments {
                check_expr(x, state, scopes, imports);
            }
        }
        Expression::Identifier(x) => {
            check_id(x, state, scopes);
        }
        Expression::Literal(_) => {}
        Expression::NsName(x) => {
            check_ns_id(x, state, scopes, imports);
        }
        Expression::PostfixOperator {
            operator: _,
            operand,
        } => {
            check_expr(operand, state, scopes, imports);
        }
        Expression::PrefixOperator {
            operator: _,
            operand,
        } => {
            check_expr(operand, state, scopes, imports);
        }
        Expression::Sizeof { argument } => {
            match argument.as_ref() {
                SizeofArgument::Typename(_) => {
                    //
                }
                SizeofArgument::Expression(x) => {
                    check_expr(&x, state, scopes, imports);
                }
            }
        }
    }
}

fn check_ns_id(
    x: &NsName,
    state: &mut State,
    scopes: &mut Vec<Scope>,
    imports: &HashMap<String, &Exports>,
) {
    match x.namespace.as_str() {
        "OS" => {}
        "" => {
            let k = x.name.as_str();
            let n = scopes.len();
            for i in 0..n {
                let scope = &mut scopes[n - i - 1];
                match scope.vars.get_mut(k) {
                    Some(x) => {
                        x.read = true;
                        return;
                    }
                    None => {}
                }
            }

            match state.root_scope.vars.get_mut(k) {
                Some(x) => {
                    x.read = true;
                    return;
                }
                None => {}
            }
            if state.root_scope.pre.contains(&x.name) {
                return;
            }
            match state.root_scope.consts.get_mut(k) {
                Some(x) => {
                    x.read = true;
                    return;
                }
                None => {}
            }
            if state.root_scope.funcs.contains_key(k) {
                return;
            }
            if state.root_scope.types.contains_key(k) {
                return;
            }
            state.errors.push(Error {
                message: format!("unknown identifier: {}", x.name),
                pos: x.pos.clone(),
            });
        }
        _ => {
            match state.root_scope.imports.get_mut(&x.namespace) {
                Some(imp) => {
                    imp.read = true;
                }
                None => {}
            }
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
        }
    }
}

fn check_id(x: &Identifier, state: &mut State, scopes: &mut Vec<Scope>) {
    let k = x.name.as_str();
    let n = scopes.len();
    for i in 0..n {
        let scope = &mut scopes[n - i - 1];
        match scope.vars.get_mut(k) {
            Some(x) => {
                x.read = true;
                return;
            }
            None => {}
        }
    }
    match state.root_scope.vars.get_mut(k) {
        Some(x) => {
            x.read = true;
            return;
        }
        None => {}
    }
    if state.root_scope.pre.contains(&x.name) {
        return;
    }
    match state.root_scope.consts.get_mut(k) {
        Some(x) => {
            x.read = true;
            return;
        }
        None => {}
    }
    match state.root_scope.funcs.get_mut(k) {
        Some(x) => {
            x.read = true;
            return;
        }
        None => {}
    }
    state.errors.push(Error {
        message: format!("unknown identifier: {}", x.name),
        pos: x.pos.clone(),
    });
}

fn infer_type(e: &Expression, scopes: &Vec<Scope>) -> Option<Typename> {
    match e {
        Expression::ArrayIndex { array: _, index: _ } => None,
        Expression::BinaryOp { op: _, a: _, b: _ } => None,
        Expression::FieldAccess {
            op: _,
            target,
            field_name: _,
        } => match infer_type(target, scopes) {
            Some(t) => {
                if t.name.namespace == "" {
                    Some(t)
                } else {
                    None
                }
            }
            None => None,
        },
        Expression::Cast {
            type_name,
            operand: _,
        } => Some(type_name.type_name.clone()),
        Expression::CompositeLiteral(_) => None,
        Expression::FunctionCall {
            function: _,
            arguments: _,
        } => None,
        Expression::Identifier(x) => match find_var(scopes, &x.name) {
            Some(v) => {
                let mut t = v.typename.clone();
                for _ in v.form.indexes {
                    t.name.name += "*";
                }
                t.name.name += &v.form.stars;
                return Some(t);
            }
            None => return None,
        },
        Expression::Literal(_) => None,
        Expression::NsName(_) => None,
        Expression::PostfixOperator {
            operator: _,
            operand: _,
        } => None,
        Expression::PrefixOperator {
            operator: _,
            operand: _,
        } => None,
        Expression::Sizeof { argument: _ } => Some(Typename {
            is_const: true,
            name: NsName {
                name: String::from("size_t"),
                namespace: String::from(""),
                pos: Pos { col: 0, line: 0 },
            },
        }),
    }
}
