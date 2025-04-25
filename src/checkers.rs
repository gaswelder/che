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
    scopestack: Vec<Scope>,
}

fn track_type_usage(state: &mut VisitorState, t: &Typename) {
    let tn = &t.name;
    // Not checking imported types.
    if tn.namespace != "" {
        return;
    }
    match state.root_scope.types.get_mut(&tn.name) {
        Some(t) => {
            t.read = true;
        }
        None => {}
    }
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
        scopestack: Vec::new(),
    };

    for e in &m.elements {
        check_module_object(e, &mut state);
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

fn check_module_object(e: &ModElem, state: &mut VisitorState) {
    match e {
        ModElem::Import(_) => {}
        ModElem::Macro { .. } => {}
        ModElem::Enum(x) => check_enumdef(x, state),
        ModElem::Typedef(x) => check_nsname(&x.type_name.name, state),
        ModElem::StructAliasTypedef { .. } => {}
        ModElem::StructTypedef(x) => check_structdef(x, state),
        ModElem::ModuleVariable(x) => check_modvar(x, state),
        ModElem::FunctionDeclaration(f) => check_function_declaration(f, state),
    }
}

fn check_body(body: &Body, state: &mut VisitorState) {
    state.scopestack.push(newscope());
    for s in &body.statements {
        match s {
            Statement::Break => {}
            Statement::Continue => {}
            Statement::For(x) => check_for(x, state),
            Statement::If(x) => check_if(x, state),
            Statement::Return(x) => check_return(x, state),
            Statement::Switch(x) => check_switch(x, state),
            Statement::VariableDeclaration(x) => check_vardecl(x, state),
            Statement::While(x) => check_while(x, state),
            Statement::Expression(e) => {
                check_expr(e, state);
            }
        }
    }
    let s = state.scopestack.pop().unwrap();
    for (k, v) in s.vars {
        if !v.read {
            state.errors.push(Error {
                message: format!("unused variable: {}", k),
                pos: v.val.pos,
            });
        }
    }
}

fn check_enumdef(x: &Enum, state: &mut VisitorState) {
    for m in &x.members {
        match &m.value {
            Some(v) => {
                check_expr(v, state);
            }
            None => {}
        }
    }
}

fn check_structdef(x: &StructTypedef, state: &mut VisitorState) {
    for f in &x.fields {
        match f {
            StructEntry::Plain(x) => {
                track_type_usage(state, &x.type_name);
                check_nsname(&x.type_name.name, state);
                for f in &x.forms {
                    for i in &f.indexes {
                        match i {
                            Some(e) => {
                                check_expr(e, state);
                            }
                            None => {}
                        }
                    }
                }
            }
            StructEntry::Union(x) => {
                for f in &x.fields {
                    track_type_usage(state, &f.type_name)
                }
            }
        }
    }
}

fn check_function_declaration(f: &FunctionDeclaration, state: &mut VisitorState) {
    track_type_usage(state, &f.type_name);
    check_nsname(&f.type_name.name, state);

    state.scopestack.push(newscope());

    for params in &f.parameters.list {
        // void a, *b is an error.
        // void *a, *b is ok.
        if is_void(&params.type_name) {
            for f in &params.forms {
                if f.hops == 0 {
                    state.errors.push(Error {
                        message: format!("can't use void as an argument type"),
                        pos: params.pos.clone(),
                    })
                }
            }
        }
        check_nsname(&params.type_name.name, state);
        for f in &params.forms {
            let n = state.scopestack.len();
            state.scopestack[n - 1].vars.insert(
                f.name.clone(),
                ScopeItem1 {
                    read: false,
                    val: VarInfo {
                        pos: f.pos.clone(),
                        typename: params.type_name.clone(),
                        form: f.clone(),
                    },
                },
            );
        }
    }
    check_body(&f.body, state);
    let s = state.scopestack.pop().unwrap();
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

fn check_return(x: &Return, state: &mut VisitorState) {
    let expression = &x.expression;
    match expression {
        Some(x) => {
            check_expr(&x, state);
        }
        None => {}
    }
}

fn check_if(x: &If, state: &mut VisitorState) {
    let condition = &x.condition;
    let body = &x.body;
    let else_body = &x.else_body;
    check_expr(condition, state);
    check_body(body, state);
    if else_body.is_some() {
        check_body(else_body.as_ref().unwrap(), state);
    }
}

fn check_for(x: &For, state: &mut VisitorState) {
    let init = &x.init;
    let condition = &x.condition;
    let action = &x.action;
    let body = &x.body;
    state.scopestack.push(newscope());
    let n = state.scopestack.len();
    match init {
        Some(init) => match init {
            ForInit::LoopCounterDeclaration {
                type_name,
                form,
                value,
            } => {
                check_nsname(&type_name.name, state);
                check_expr(value, state);
                state.scopestack[n - 1].vars.insert(
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
                check_expr(x, state);
            }
        },
        None => {}
    }
    match condition {
        Some(condition) => {
            check_expr(condition, state);
        }
        None => {}
    }
    match action {
        Some(action) => {
            check_expr(action, state);
        }
        None => {}
    }
    check_body(body, state);
    state.scopestack.pop();
}

fn check_switch(x: &Switch, state: &mut VisitorState) {
    let cases = &x.cases;
    let default = &x.default_case;
    check_expr(&x.value, state);
    for c in cases {
        for v in &c.values {
            match v {
                SwitchCaseValue::Identifier(x) => {
                    check_nsname(x, state);
                }
                SwitchCaseValue::Literal(_) => {}
            }
        }
        check_body(&c.body, state);
    }
    if default.is_some() {
        check_body(default.as_ref().unwrap(), state);
    }
}

fn check_while(x: &While, state: &mut VisitorState) {
    check_expr(&x.condition, state);
    check_body(&x.body, state);
}

fn pushvar(state: &mut VisitorState, x: &VariableDeclaration) {
    let n = state.scopestack.len();
    state.scopestack[n - 1].vars.insert(
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

fn check_modvar(x: &VariableDeclaration, state: &mut VisitorState) {
    track_type_usage(state, &x.type_name);

    // Array declaration? Look into the index expressions.
    for i in &x.form.indexes {
        match i {
            Some(e) => {
                check_expr(e, state);
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
    check_nsname(&x.type_name.name, state);
    check_expr(x.value.as_ref().unwrap(), state);
}

// Checks int foo = val.
fn check_vardecl(x: &VariableDeclaration, state: &mut VisitorState) {
    track_type_usage(state, &x.type_name);

    // Check ... in "int foo[...]".
    for i in &x.form.indexes {
        match i {
            Some(e) => {
                check_expr(e, state);
            }
            None => {}
        }
    }

    check_nsname(&x.type_name.name, state);
    pushvar(state, x);
    if x.value.is_some() {
        check_expr(x.value.as_ref().unwrap(), state);
    }
}

fn check_expr(e: &Expression, state: &mut VisitorState) -> Type {
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
            check_id(x, state);
            match find_var(&state.scopestack, &x.name) {
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
            check_nsname(x, state);
            Type::Unknown
        }
        Expression::ArrayIndex(x) => {
            let array = &x.array;
            let index = &x.index;
            let t1 = check_expr(array, state);
            check_expr(index, state);
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
            let t1 = check_expr(a, state);
            let t2 = check_expr(b, state);
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
            let stype = check_expr(target, state);
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
            check_nsname(&type_name.type_name.name, state);
            check_expr(operand, state);
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
                check_expr(&e.value, state);
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
            let rettype = check_expr(function, state);
            for x in arguments {
                check_expr(x, state);
            }
            return rettype;
        }
        Expression::PostfixOperator(x) => {
            let operand = &x.operand;
            let operator = &x.operator;
            let t = check_expr(operand, state);
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
            let t = check_expr(operand, state);
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
                    check_expr(&x, state);
                }
            }
            Type::Bytes(types::Bytes {
                sign: types::Signedness::Signed,
                size: 0,
            })
        }
    }
}

fn check_nsname(x: &NsName, state: &mut VisitorState) {
    match x.namespace.as_str() {
        "OS" => {}
        "" => {
            let k = x.name.as_str();
            let n = state.scopestack.len();
            for i in 0..n {
                let scope = &mut state.scopestack[n - i - 1];
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

fn check_id(x: &Identifier, state: &mut VisitorState) {
    let k = x.name.as_str();
    let n = state.scopestack.len();
    for i in 0..n {
        let scope = &mut state.scopestack[n - i - 1];
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
