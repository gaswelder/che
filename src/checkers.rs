use crate::{c, exports::Exports, nodes::*, parser::Error, resolve::getns};
use std::collections::{HashMap, HashSet};

struct StrPos {
    str: String,
    pos: String,
}

struct State {
    errors: Vec<Error>,
}

#[derive(Clone, Debug)]
struct ScopeItem {
    read: bool,
    pos: String,
    ispub: bool,
}

#[derive(Clone, Debug)]
struct ScopeItem1<T> {
    read: bool,
    val: T,
}

#[derive(Clone, Debug)]
struct Scope {
    pre: Vec<String>,
    imports: HashMap<String, ScopeItem1<ImportNode>>,
    types: HashMap<String, ScopeItem>,
    consts: HashMap<String, ScopeItem>,
    vars: HashMap<String, ScopeItem>,
    funcs: HashMap<String, ScopeItem1<FunctionDeclaration>>,
}

fn newscope() -> Scope {
    return Scope {
        pre: vec![],
        imports: HashMap::new(),
        consts: HashMap::new(),
        vars: HashMap::new(),
        funcs: HashMap::new(),
        types: HashMap::new(),
    };
}

pub fn run(m: &Module, imports: &HashMap<String, &Exports>) -> Vec<Error> {
    let mut declared_local_types = Vec::new();
    let mut used_local_types = HashSet::new();
    let mut state = State { errors: Vec::new() };
    let mut scopestack = vec![get_module_scope(m)];

    for e in &m.elements {
        check_module_object(
            e,
            &mut state,
            &mut scopestack,
            &mut declared_local_types,
            &mut used_local_types,
            imports,
        );
    }
    for t in declared_local_types {
        if !used_local_types.contains(&t.str) {
            state.errors.push(Error {
                message: format!("unused type: {}", t.str),
                pos: t.pos.clone(),
            })
        }
    }

    let s = scopestack.pop().unwrap();
    for (k, v) in s.imports {
        if !v.read {
            state.errors.push(Error {
                message: format!("unused import: {}", k),
                pos: v.val.pos.clone(),
            })
        }
    }
    for (k, v) in s.consts {
        if !v.ispub && !v.read {
            state.errors.push(Error {
                message: format!("unused constant: {}", k),
                pos: v.pos,
            });
        }
    }
    for (k, v) in s.funcs {
        if !v.val.is_pub && !v.read && k != "main" {
            state.errors.push(Error {
                message: format!("unused function: {}", k),
                pos: format!("{}", v.val.pos),
            });
        }
    }

    return state.errors;
}

fn check_module_object(
    e: &ModuleObject,
    state: &mut State,
    scopestack: &mut Vec<Scope>,
    declared_local_types: &mut Vec<StrPos>,
    used_local_types: &mut HashSet<String>,
    imports: &HashMap<String, &Exports>,
) {
    match e {
        ModuleObject::Import(_) => {}
        ModuleObject::Typedef(x) => {
            if !x.is_pub {
                declared_local_types.push(StrPos {
                    str: x.alias.name.clone(),
                    pos: x.pos.clone(),
                });
            }
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
            check_function_declaration(f, state, scopestack, imports, used_local_types);
        }
        ModuleObject::Macro { .. } => {}
        ModuleObject::ModuleVariable {
            type_name,
            form,
            value,
            pos,
        } => {
            // Local type? Register the usage.
            let n = &type_name.name;
            if n.namespace == "" {
                used_local_types.insert(n.name.clone());
            }

            // Array declaration? Look into the index expressions.
            for i in &form.indexes {
                match i {
                    Some(e) => check_expr(e, state, scopestack, imports),
                    None => {}
                }
            }

            if has_function_call(value) {
                state.errors.push(Error {
                    message: format!("function call in module variable initialization"),
                    pos: format!("{}", pos),
                })
            }
            check_ns_id(&type_name.name, state, scopestack, imports);
            check_expr(value, state, scopestack, imports);
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
                                used_local_types.insert(n.name.clone());
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
    used_local_types: &mut HashSet<String>,
) {
    let tn = &f.type_name.name;
    if tn.namespace == "" {
        used_local_types.insert(tn.name.clone());
    }

    check_ns_id(&f.type_name.name, state, scopestack, imports);

    scopestack.push(newscope());

    for tf in &f.parameters.list {
        let isvoid = tf.type_name.name.namespace == "" && tf.type_name.name.name == "void";
        if isvoid {
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
                ScopeItem {
                    read: false,
                    pos: tf.pos.clone(),
                    ispub: false,
                },
            );
        }
    }
    check_body(&f.body, state, scopestack, imports, used_local_types);
    let s = scopestack.pop().unwrap();
    for (k, v) in s.vars {
        if !v.read {
            state.errors.push(Error {
                message: format!("{} not used", k),
                pos: v.pos,
            })
        }
    }
}

fn has_function_call(e: &Expression) -> bool {
    return match e {
        Expression::ArrayIndex { array, index } => {
            has_function_call(array) || has_function_call(index)
        }
        Expression::BinaryOp { op: _, a, b } => has_function_call(a) || has_function_call(b),
        Expression::FieldAccess {
            op: _,
            target,
            field_name: _,
        } => has_function_call(target),
        Expression::Cast {
            type_name: _,
            operand,
        } => has_function_call(operand),
        Expression::CompositeLiteral(_) => false,
        Expression::FunctionCall {
            function: _,
            arguments: _,
        } => true,
        Expression::Identifier(_) => false,
        Expression::Literal(_) => false,
        Expression::NsName(_) => false,
        Expression::PostfixOperator {
            operator: _,
            operand,
        } => has_function_call(operand),
        Expression::PrefixOperator {
            operator: _,
            operand,
        } => has_function_call(operand),
        Expression::Sizeof { argument: _ } => false,
    };
}

fn get_module_scope(m: &Module) -> Scope {
    let mut s = Scope {
        pre: vec![
            String::from("false"),
            String::from("NULL"),
            String::from("true"),
            // custom
            String::from("nelem"),
            String::from("panic"),
            String::from("min"),
            String::from("max"),
        ],
        imports: HashMap::new(),
        consts: HashMap::new(),
        vars: HashMap::new(),
        funcs: HashMap::new(),
        types: HashMap::new(),
    };
    // let mut scope = vec![
    //     // should be fixed
    //     "break", "continue", "false", "NULL", "true", //
    //     // custom
    //     "nelem", "panic", "min", "max",
    // ];
    for c in c::CCONST {
        s.pre.push(c.to_string());
    }
    for c in c::CFUNCS {
        s.pre.push(c.to_string());
    }
    for c in c::CTYPES {
        s.pre.push(c.to_string());
    }

    for e in &m.elements {
        match e {
            ModuleObject::Import(x) => {
                let ns = getns(&x.specified_path);
                s.imports.insert(
                    ns,
                    ScopeItem1 {
                        read: false,
                        val: x.clone(),
                    },
                );
            }
            ModuleObject::FunctionDeclaration(f) => {
                s.funcs.insert(
                    f.form.name.clone(),
                    ScopeItem1 {
                        read: false,
                        val: f.clone(),
                    },
                );
            }
            ModuleObject::Enum {
                is_pub,
                members,
                pos: _,
            } => {
                for m in members {
                    s.consts.insert(
                        m.id.name.clone(),
                        ScopeItem {
                            read: false,
                            pos: m.pos.clone(),
                            ispub: *is_pub,
                        },
                    );
                }
            }
            ModuleObject::Macro { name, value, pos } => {
                if name == "define" {
                    let mut parts = value.trim().split(" ");
                    s.consts.insert(
                        String::from(parts.next().unwrap()),
                        ScopeItem {
                            read: false,
                            pos: String::from(pos),
                            ispub: false,
                        },
                    );
                }
                if name == "type" {
                    s.pre.push(String::from(value.trim()));
                }
            }
            ModuleObject::ModuleVariable {
                type_name: _,
                form,
                value: _,
                pos,
            } => {
                s.vars.insert(
                    form.name.clone(),
                    ScopeItem {
                        read: false,
                        pos: pos.clone(),
                        ispub: false,
                    },
                );
            }
            ModuleObject::StructTypedef(x) => {
                s.types.insert(
                    x.name.name.clone(),
                    ScopeItem {
                        read: false,
                        pos: x.pos.clone(),
                        ispub: x.is_pub,
                    },
                );
            }
            ModuleObject::Typedef(x) => {
                s.types.insert(
                    x.alias.name.clone(),
                    ScopeItem {
                        read: false,
                        pos: x.pos.clone(),
                        ispub: x.is_pub,
                    },
                );
            }
            ModuleObject::StructAliasTypedef {
                pos,
                is_pub,
                struct_name: _,
                type_alias,
            } => {
                s.types.insert(
                    type_alias.clone(),
                    ScopeItem {
                        read: false,
                        pos: pos.clone(),
                        ispub: *is_pub,
                    },
                );
            }
        }
    }
    return s;
}

fn check_body(
    body: &Body,
    state: &mut State,
    scopes: &mut Vec<Scope>,
    imports: &HashMap<String, &Exports>,
    used_types: &mut HashSet<String>,
) {
    scopes.push(newscope());
    for s in &body.statements {
        match s {
            Statement::Break => {}
            Statement::Continue => {}
            Statement::Expression(e) => {
                check_expr(e, state, scopes, imports);
            }
            Statement::Panic { arguments, pos: _ } => {
                for e in arguments {
                    check_expr(e, state, scopes, imports);
                }
            }
            Statement::For {
                init,
                condition,
                action,
                body,
            } => {
                scopes.push(newscope());
                let n = scopes.len();
                match init {
                    Some(init) => match init {
                        ForInit::LoopCounterDeclaration {
                            type_name,
                            form,
                            value,
                        } => {
                            check_ns_id(&type_name.name, state, scopes, imports);
                            check_expr(value, state, scopes, imports);
                            scopes[n - 1].vars.insert(
                                form.name.clone(),
                                ScopeItem {
                                    read: true,
                                    pos: String::from("for"),
                                    ispub: false,
                                },
                            );
                        }
                        ForInit::Expression(x) => {
                            check_expr(x, state, scopes, imports);
                        }
                    },
                    None => {}
                }
                match condition {
                    Some(condition) => {
                        check_expr(condition, state, scopes, imports);
                    }
                    None => {}
                }
                match action {
                    Some(action) => {
                        check_expr(action, state, scopes, imports);
                    }
                    None => {}
                }
                check_body(body, state, scopes, imports, used_types);
                scopes.pop();
            }
            Statement::If {
                condition,
                body,
                else_body,
            } => {
                check_expr(condition, state, scopes, imports);
                check_body(body, state, scopes, imports, used_types);
                if else_body.is_some() {
                    check_body(
                        else_body.as_ref().unwrap(),
                        state,
                        scopes,
                        imports,
                        used_types,
                    );
                }
            }
            Statement::Return { expression } => match expression {
                Some(x) => {
                    check_expr(x, state, scopes, imports);
                }
                None => {}
            },
            Statement::Switch {
                value,
                cases,
                default_case: default,
            } => {
                check_expr(value, state, scopes, imports);
                for c in cases {
                    for v in &c.values {
                        match v {
                            SwitchCaseValue::Identifier(x) => {
                                check_ns_id(x, state, scopes, imports);
                            }
                            SwitchCaseValue::Literal(_) => {}
                        }
                    }
                    check_body(&c.body, state, scopes, imports, used_types);
                }
                if default.is_some() {
                    check_body(
                        default.as_ref().unwrap(),
                        state,
                        scopes,
                        imports,
                        used_types,
                    );
                }
            }
            Statement::VariableDeclaration {
                type_name,
                form,
                value,
                pos,
            } => {
                // Local type? Count as usage.
                if type_name.name.namespace == "" {
                    used_types.insert(type_name.name.name.clone());
                }

                // If it's a variable declaration,
                // look into the index expressions.
                for i in &form.indexes {
                    match i {
                        Some(e) => {
                            check_expr(e, state, scopes, imports);
                        }
                        None => {}
                    }
                }

                check_ns_id(&type_name.name, state, scopes, imports);
                let n = scopes.len();
                if value.is_some() {
                    scopes[n - 1].vars.insert(
                        form.name.clone(),
                        ScopeItem {
                            read: false,
                            pos: pos.clone(),
                            ispub: false,
                        },
                    );
                    check_expr(value.as_ref().unwrap(), state, scopes, imports);
                } else {
                    scopes[n - 1].vars.insert(
                        form.name.clone(),
                        ScopeItem {
                            read: false,
                            pos: pos.clone(),
                            ispub: false,
                        },
                    );
                }
            }
            Statement::While { condition, body } => {
                check_expr(condition, state, scopes, imports);
                check_body(body, state, scopes, imports, used_types);
            }
        }
    }
    let s = scopes.pop().unwrap();
    for (k, v) in s.vars {
        if !v.read {
            state.errors.push(Error {
                message: format!("unused variable: {}", k),
                pos: v.pos,
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
        Expression::BinaryOp { op: _, a, b } => {
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
                Expression::Identifier(x) => match scopes[0].funcs.get_mut(&x.name) {
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
                if scope.pre.contains(&x.name) {
                    return;
                }
                match scope.consts.get_mut(k) {
                    Some(x) => {
                        x.read = true;
                        return;
                    }
                    None => {}
                }
                if scope.funcs.contains_key(k) {
                    return;
                }
                if scope.types.contains_key(k) {
                    return;
                }
            }
            state.errors.push(Error {
                message: format!("unknown identifier: {}", x.name),
                pos: x.pos.clone(),
            });
        }
        _ => {
            let s = &mut scopes[0];
            match s.imports.get_mut(&x.namespace) {
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
        if scope.pre.contains(&x.name) {
            return;
        }
        match scope.consts.get_mut(k) {
            Some(x) => {
                x.read = true;
                return;
            }
            None => {}
        }
        match scope.funcs.get_mut(k) {
            Some(x) => {
                x.read = true;
                return;
            }
            None => {}
        }
    }
    state.errors.push(Error {
        message: format!("unknown identifier: {}", x.name),
        pos: x.pos.clone(),
    });
}
