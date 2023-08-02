use crate::{
    exports::Exports,
    node_queries::{body_returns, expression_pos, has_function_call, isvoid},
    nodes::*,
    parser::Error,
    scopes::{find_var, get_module_scope, newscope, RootScope, Scope, ScopeItem1, VarInfo},
    types::{self, Type},
};
use std::collections::HashMap;

struct State {
    errors: Vec<Error>,
    type_errors: Vec<Error>,
    root_scope: RootScope,
}

pub fn run(m: &Module, imports: &HashMap<String, &Exports>) -> Vec<Error> {
    let mut state = State {
        errors: Vec::new(),
        type_errors: Vec::new(),
        root_scope: get_module_scope(m),
    };
    let mut scopestack: Vec<Scope> = Vec::new();

    for e in &m.elements {
        check_module_object(e, &mut state, &mut scopestack, imports);
    }

    for (k, v) in &state.root_scope.imports {
        if !v.read {
            state.errors.push(Error {
                message: format!("unused import: {}", k),
                pos: v.val.pos.clone(),
            })
        }
    }
    for (k, v) in &state.root_scope.types {
        let ispub;
        let mut _pos;
        match &v.val {
            ModuleObject::Typedef(x) => {
                ispub = x.is_pub;
                _pos = x.pos.clone();
            }
            ModuleObject::StructAliasTypedef {
                pos,
                is_pub,
                struct_name: _,
                type_alias: _,
            } => {
                ispub = *is_pub;
                _pos = pos.clone();
            }
            ModuleObject::StructTypedef(x) => {
                ispub = x.is_pub;
                _pos = x.pos.clone();
            }
            _ => panic!("unexpected object in scope types"),
        }
        if !ispub && !v.read {
            state.errors.push(Error {
                message: format!("unused type: {}", k),
                pos: _pos.clone(),
            })
        }
    }
    for (k, v) in &state.root_scope.consts {
        if !v.ispub && !v.read {
            state.errors.push(Error {
                message: format!("unused constant: {}", k),
                pos: v.pos.clone(),
            });
        }
    }
    for (k, v) in &state.root_scope.funcs {
        if !v.val.is_pub && !v.read && k != "main" {
            state.errors.push(Error {
                message: format!("unused function: {}", k),
                pos: v.val.pos.clone(),
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
                    Some(v) => {
                        check_expr(v, state, scopestack, imports);
                    }
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
                    Some(e) => {
                        check_expr(e, state, scopestack, imports);
                    }
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
) -> Type {
    match e {
        Expression::ArrayIndex { array, index } => {
            let t1 = check_expr(array, state, scopes, imports);
            check_expr(index, state, scopes, imports);
            match types::deref(t1) {
                Ok(r) => r,
                Err(err) => {
                    state.type_errors.push(Error {
                        message: err,
                        pos: expression_pos(e),
                    });
                    return Type::Opaque;
                }
            }
        }
        Expression::BinaryOp { op: _, a, b } => {
            let t1 = check_expr(a, state, scopes, imports);
            let t2 = check_expr(b, state, scopes, imports);
            match types::sum(t1, t2) {
                Ok(r) => r,
                Err(err) => {
                    state.type_errors.push(Error {
                        message: err,
                        pos: expression_pos(e),
                    });
                    return Type::Opaque;
                }
            }
        }
        Expression::FieldAccess {
            op: _,
            target,
            field_name,
        } => {
            let stype = check_expr(target, state, scopes, imports);
            match types::access(stype, field_name, &state.root_scope) {
                Ok(r) => r,
                Err(err) => {
                    state.type_errors.push(Error {
                        message: err,
                        pos: expression_pos(e),
                    });
                    return Type::Opaque;
                }
            }
        }
        Expression::Cast { type_name, operand } => {
            check_ns_id(&type_name.type_name.name, state, scopes, imports);
            check_expr(operand, state, scopes, imports);
            match types::get_type(&type_name.type_name, &state.root_scope) {
                Ok(r) => r,
                Err(err) => {
                    state.type_errors.push(Error {
                        message: err,
                        pos: expression_pos(e),
                    });
                    return Type::Opaque;
                }
            }
        }
        Expression::CompositeLiteral(x) => {
            for e in &x.entries {
                check_expr(&e.value, state, scopes, imports);
            }
            Type::Opaque
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
            let rettype = check_expr(function, state, scopes, imports);
            for x in arguments {
                check_expr(x, state, scopes, imports);
            }
            return rettype;
        }
        Expression::Identifier(x) => {
            check_id(x, state, scopes);
            match find_var(scopes, &x.name) {
                Some(v) => match types::get_type(&v.typename, &state.root_scope) {
                    Ok(t) => {
                        return t;
                    }
                    Err(err) => {
                        state.type_errors.push(Error {
                            message: err,
                            pos: expression_pos(e),
                        });
                        return Type::Opaque;
                    }
                },
                None => {}
            }
            match types::find_stdlib(&x.name) {
                Some(v) => {
                    return v;
                }
                None => return Type::Opaque,
            }
        }
        Expression::Literal(x) => match x {
            Literal::Char(_) => Type::Bytes(types::Bytes {
                sign: types::Signedness::Unknonwn,
                size: 1,
            }),
            Literal::String(_) => types::addr(Type::Bytes(types::Bytes {
                sign: types::Signedness::Unknonwn,
                size: 1,
            })),
            Literal::Number(_) => Type::Bytes(types::Bytes {
                sign: types::Signedness::Unknonwn,
                size: 0,
            }),
            Literal::Null => Type::Null,
        },
        Expression::NsName(x) => {
            check_ns_id(x, state, scopes, imports);
            Type::Opaque
        }
        Expression::PostfixOperator { operator, operand } => {
            let t = check_expr(operand, state, scopes, imports);
            match operator.as_str() {
                "++" | "--" => t,
                _ => {
                    dbg!(operator);
                    todo!();
                }
            }
        }
        Expression::PrefixOperator { operator, operand } => {
            let t = check_expr(operand, state, scopes, imports);
            match operator.as_str() {
                "++" | "--" => t,
                "!" | "-" | "~" => t,
                "*" => match types::deref(t) {
                    Ok(r) => r,
                    Err(err) => {
                        state.type_errors.push(Error {
                            message: err,
                            pos: expression_pos(e),
                        });
                        return Type::Opaque;
                    }
                },
                "&" => types::addr(t),
                _ => {
                    dbg!(operator);
                    todo!();
                }
            }
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
            Type::Bytes(types::Bytes {
                sign: types::Signedness::Signed,
                size: 0,
            })
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
