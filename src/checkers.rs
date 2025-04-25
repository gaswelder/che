use crate::{
    errors::Error,
    exports::Exports,
    format_che::format_expression,
    node_queries::{body_returns, expression_pos, has_function_call},
    nodes::*,
    scopes::{find_var, get_module_scope, newscope, RootScope, Scope, ScopeItem1, VarInfo},
    types::{self, Type},
};
use std::{collections::HashMap, env};

const DUMP_TYPES: bool = false;

struct VisitorState {
    errors: Vec<Error>,
    type_errors: Vec<Error>,
    root_scope: RootScope,
    imports: HashMap<String, Exports>,
    // scopestack: Vec<Scope>,
}

pub fn run(m: &Module, imports: &HashMap<String, &Exports>) -> Vec<Error> {
    let mut imps = HashMap::new();
    for (k, v) in imports.iter() {
        imps.insert(k.clone(), (**v).clone());
    }

    let mut state = VisitorState {
        errors: Vec::new(),
        type_errors: Vec::new(),
        root_scope: get_module_scope(m),
        imports: imps,
    };
    let mut scopestack: Vec<Scope> = Vec::new();

    for e in &m.elements {
        check_module_object(e, &mut state, &mut scopestack);
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
            ModElem::Typedef(x) => {
                ispub = x.is_pub;
                _pos = x.pos.clone();
            }
            ModElem::StructAliasTypedef(x) => {
                ispub = x.is_pub;
                _pos = x.pos.clone();
            }
            ModElem::StructTypedef(x) => {
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
    if env::var("TYPES").is_ok() {
        if state.type_errors.len() > 0 {
            return state.type_errors;
        }
    }
    return state.errors;
}

fn check_module_object(e: &ModElem, state: &mut VisitorState, scopestack: &mut Vec<Scope>) {
    match e {
        ModElem::Import(_) => {}
        ModElem::Macro { .. } => {}
        ModElem::Enum(x) => {
            for m in &x.members {
                match &m.value {
                    Some(v) => {
                        check_expr(v, state, scopestack);
                    }
                    None => {}
                }
            }
        }
        ModElem::Typedef(x) => {
            check_ns_id(&x.type_name.name, state, scopestack);
        }
        ModElem::StructAliasTypedef { .. } => {}
        ModElem::StructTypedef(x) => {
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
                        check_ns_id(&x.type_name.name, state, scopestack);
                        for f in &x.forms {
                            for i in &f.indexes {
                                match i {
                                    Some(e) => {
                                        check_expr(e, state, scopestack);
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
        ModElem::ModuleVariable(x) => {
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
                        check_expr(e, state, scopestack);
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
            check_ns_id(&x.type_name.name, state, scopestack);
            check_expr(x.value.as_ref().unwrap(), state, scopestack);
        }
        ModElem::FunctionDeclaration(f) => {
            check_function_declaration(f, state, scopestack);
        }
    }
}

fn check_function_declaration(
    f: &FunctionDeclaration,
    state: &mut VisitorState,
    scopestack: &mut Vec<Scope>,
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

    check_ns_id(&f.type_name.name, state, scopestack);

    scopestack.push(newscope());

    for tf in &f.parameters.list {
        if is_void(&tf.type_name) {
            for f in &tf.forms {
                if f.hops == 0 {
                    state.errors.push(Error {
                        message: format!("can't use void as an argument type"),
                        pos: tf.pos.clone(),
                    })
                }
            }
        }
        check_ns_id(&tf.type_name.name, state, scopestack);
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
    check_body(&f.body, state, scopestack);
    let s = scopestack.pop().unwrap();
    for (k, v) in s.vars {
        if !v.read {
            state.errors.push(Error {
                message: format!("{} not used", k),
                pos: v.val.pos,
            })
        }
    }

    if (!is_void(&f.type_name) || f.form.hops == 1) && !body_returns(&f.body) {
        state.errors.push(Error {
            message: format!("{}: missing return", f.form.name),
            pos: f.pos.clone(),
        })
    }
}

fn check_body(body: &Body, state: &mut VisitorState, scopestack: &mut Vec<Scope>) {
    scopestack.push(newscope());
    for s in &body.statements {
        match s {
            Statement::Break => {}
            Statement::Continue => {}
            Statement::Expression(e) => {
                check_expr(e, state, scopestack);
            }
            Statement::For(x) => {
                let init = &x.init;
                let condition = &x.condition;
                let action = &x.action;
                let body = &x.body;
                scopestack.push(newscope());
                let n = scopestack.len();
                match init {
                    Some(init) => match init {
                        ForInit::LoopCounterDeclaration {
                            type_name,
                            form,
                            value,
                        } => {
                            check_ns_id(&type_name.name, state, scopestack);
                            check_expr(value, state, scopestack);
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
                            check_expr(x, state, scopestack);
                        }
                    },
                    None => {}
                }
                match condition {
                    Some(condition) => {
                        check_expr(condition, state, scopestack);
                    }
                    None => {}
                }
                match action {
                    Some(action) => {
                        check_expr(action, state, scopestack);
                    }
                    None => {}
                }
                check_body(body, state, scopestack);
                scopestack.pop();
            }
            Statement::If(x) => {
                let condition = &x.condition;
                let body = &x.body;
                let else_body = &x.else_body;
                check_expr(condition, state, scopestack);
                check_body(body, state, scopestack);
                if else_body.is_some() {
                    check_body(else_body.as_ref().unwrap(), state, scopestack);
                }
            }
            Statement::Return(x) => {
                let expression = &x.expression;
                match expression {
                    Some(x) => {
                        check_expr(&x, state, scopestack);
                    }
                    None => {}
                }
            }
            Statement::Switch(x) => {
                let value = &x.value;
                let cases = &x.cases;
                let default = &x.default_case;
                check_expr(value, state, scopestack);
                for c in cases {
                    for v in &c.values {
                        match v {
                            SwitchCaseValue::Identifier(x) => {
                                check_ns_id(x, state, scopestack);
                            }
                            SwitchCaseValue::Literal(_) => {}
                        }
                    }
                    check_body(&c.body, state, scopestack);
                }
                if default.is_some() {
                    check_body(default.as_ref().unwrap(), state, scopestack);
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
                            check_expr(e, state, scopestack);
                        }
                        None => {}
                    }
                }

                check_ns_id(&x.type_name.name, state, scopestack);
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
                    check_expr(x.value.as_ref().unwrap(), state, scopestack);
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
            Statement::While(x) => {
                let condition = &x.condition;
                let body = &x.body;
                check_expr(condition, state, scopestack);
                check_body(body, state, scopestack);
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

fn check_expr(e: &Expression, state: &mut VisitorState, scopes: &mut Vec<Scope>) -> Type {
    match e {
        Expression::Literal(x) => match x {
            Literal::Char(_) => Type::Bytes(types::Bytes {
                sign: types::Signedness::Unknown,
                size: 1,
            }),
            Literal::String(_) => types::addr(Type::Bytes(types::Bytes {
                sign: types::Signedness::Unknown,
                size: 1,
            })),
            Literal::Number(_) => Type::Bytes(types::Bytes {
                sign: types::Signedness::Unknown,
                size: 0,
            }),
            Literal::Null => Type::Null,
        },
        Expression::Identifier(x) => {
            check_id(x, state, scopes);
            match find_var(scopes, &x.name) {
                Some(varinfo) => match types::derive_typeform(
                    &varinfo.typename,
                    &varinfo.form,
                    &state.root_scope,
                ) {
                    Ok(t) => {
                        return t;
                    }
                    Err(err) => {
                        state.type_errors.push(Error {
                            message: err,
                            pos: expression_pos(e),
                        });
                        return Type::Unknown;
                    }
                },
                None => {}
            }
            match types::find_stdlib(&x.name) {
                Some(v) => {
                    return v;
                }
                None => return Type::Unknown,
            }
        }
        Expression::NsName(x) => {
            check_ns_id(x, state, scopes);
            Type::Unknown
        }
        Expression::ArrayIndex(x) => {
            let array = &x.array;
            let index = &x.index;
            let t1 = check_expr(array, state, scopes);
            check_expr(index, state, scopes);
            match types::deref(t1) {
                Ok(r) => r,
                Err(err) => {
                    state.type_errors.push(Error {
                        message: err,
                        pos: expression_pos(e),
                    });
                    return Type::Unknown;
                }
            }
        }
        Expression::BinaryOp(x) => {
            let a = &x.a;
            let b = &x.b;
            let t1 = check_expr(a, state, scopes);
            let t2 = check_expr(b, state, scopes);
            match types::sum(t1, t2) {
                Ok(r) => r,
                Err(err) => {
                    state.type_errors.push(Error {
                        message: err,
                        pos: expression_pos(e),
                    });
                    return Type::Unknown;
                }
            }
        }
        Expression::FieldAccess(x) => {
            let op = &x.op;
            let target = &x.target;
            let field_name = &x.field_name;
            let stype = check_expr(target, state, scopes);
            if DUMP_TYPES {
                println!("typeof {} = {}", format_expression(target), stype.fmt());
            }
            match types::access(&op, stype, field_name, &state.root_scope) {
                Ok(r) => r,
                Err(err) => {
                    state.type_errors.push(Error {
                        message: err,
                        pos: expression_pos(e),
                    });
                    return Type::Unknown;
                }
            }
        }
        Expression::Cast(x) => {
            let type_name = &x.type_name;
            let operand = &x.operand;
            check_ns_id(&type_name.type_name.name, state, scopes);
            check_expr(operand, state, scopes);
            match types::get_type(&type_name.type_name, &state.root_scope) {
                Ok(r) => r,
                Err(err) => {
                    state.type_errors.push(Error {
                        message: err,
                        pos: expression_pos(e),
                    });
                    return Type::Unknown;
                }
            }
        }
        Expression::CompositeLiteral(x) => {
            for e in &x.entries {
                check_expr(&e.value, state, scopes);
            }
            Type::Unknown
        }
        Expression::FunctionCall(x) => {
            let function = &x.function;
            let arguments = &x.arguments;
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
            let rettype = check_expr(function, state, scopes);
            for x in arguments {
                check_expr(x, state, scopes);
            }
            return rettype;
        }
        Expression::PostfixOperator(x) => {
            let operand = &x.operand;
            let operator = &x.operator;
            let t = check_expr(operand, state, scopes);
            match operator.as_str() {
                "++" | "--" => t,
                _ => {
                    dbg!(operator);
                    todo!();
                }
            }
        }
        Expression::PrefixOperator(x) => {
            let operand = &x.operand;
            let operator = &x.operator;
            let t = check_expr(operand, state, scopes);
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
                        return Type::Unknown;
                    }
                },
                "&" => types::addr(t),
                _ => {
                    dbg!(operator);
                    todo!();
                }
            }
        }
        Expression::Sizeof(x) => {
            let argument = &x.argument;
            match argument.as_ref() {
                SizeofArgument::Typename(_) => {
                    //
                }
                SizeofArgument::Expression(x) => {
                    check_expr(&x, state, scopes);
                }
            }
            Type::Bytes(types::Bytes {
                sign: types::Signedness::Signed,
                size: 0,
            })
        }
    }
}

fn check_ns_id(x: &NsName, state: &mut VisitorState, scopes: &mut Vec<Scope>) {
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
            let e = state.imports.get(&x.namespace).unwrap();
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

fn check_id(x: &Identifier, state: &mut VisitorState, scopes: &mut Vec<Scope>) {
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
