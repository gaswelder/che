use crate::nodes::*;

// Tells whether any of the exports of dep are used in m.
pub fn depused(m: &Module, dep: &Module) -> bool {
    let exports = get_exports(dep);
    let mut list: Vec<String> = Vec::new();
    for x in exports.consts {
        list.push(x.id);
    }
    for x in exports.fns {
        list.push(x.form.name);
    }
    for x in exports.types {
        list.push(x.alias);
    }
    for x in exports.structs {
        list.push(x.name);
    }
    for obj in &m.elements {
        match obj {
            ModuleObject::Macro { .. } => {}
            ModuleObject::Import { .. } => {}
            ModuleObject::Enum { is_pub: _, members } => {
                for m in members {
                    match &m.value {
                        Some(e) => {
                            if used_in_expr(e, &list, &dep.id.id) {
                                return true;
                            }
                        }
                        None => {}
                    }
                }
            }
            ModuleObject::StructAliasTypedef { .. } => {}
            ModuleObject::Typedef(Typedef {
                is_pub: _,
                type_name,
                ..
            }) => {
                if type_name.name.namespace == dep.id.id {
                    return true;
                }
            }
            ModuleObject::StructTypedef(StructTypedef { is_pub, name, .. }) => {
                if *is_pub && has(&name, &list) {
                    return true;
                }
            }
            ModuleObject::ModuleVariable {
                form: _,
                type_name,
                value,
            } => {
                if type_name.name.namespace == dep.id.id || used_in_expr(&value, &list, &dep.id.id)
                {
                    return true;
                }
            }
            ModuleObject::FunctionDeclaration(x) => {
                if x.type_name.name.namespace == dep.id.id {
                    return true;
                }
                for p in &x.parameters.list {
                    if p.type_name.name.namespace == dep.id.id {
                        return true;
                    }
                }
                if used_in_body(&x.body, &list, &dep.id.id) {
                    return true;
                }
            }
        }
    }
    return false;
}

fn used_in_expr(e: &Expression, list: &Vec<String>, dep_id: &String) -> bool {
    match e {
        Expression::Literal(_) => false,
        Expression::CompositeLiteral(CompositeLiteral { entries }) => {
            for e in entries {
                match &e.key {
                    Some(expr) => {
                        if used_in_expr(&expr, list, dep_id) {
                            return true;
                        }
                    }
                    None => {}
                }
                if used_in_expr(&e.value, list, dep_id) {
                    return true;
                }
            }
            return false;
        }
        Expression::Identifier(x) => has(x, list),
        Expression::BinaryOp { op: _, a, b } => {
            used_in_expr(a, list, dep_id) || used_in_expr(b, list, dep_id)
        }
        Expression::PrefixOperator {
            operator: _,
            operand,
        } => used_in_expr(operand, list, dep_id),
        Expression::PostfixOperator {
            operand,
            operator: _,
        } => used_in_expr(operand, list, dep_id),
        Expression::Cast { type_name, operand } => {
            type_name.type_name.name.namespace == *dep_id || used_in_expr(operand, list, dep_id)
        }
        Expression::FunctionCall {
            function,
            arguments,
        } => {
            if used_in_expr(function, list, dep_id) {
                return true;
            }
            for a in arguments {
                if used_in_expr(a, list, dep_id) {
                    return true;
                }
            }
            return false;
        }
        Expression::Sizeof { argument } => match &**argument {
            SizeofArgument::Expression(e) => used_in_expr(e, list, dep_id),
            SizeofArgument::Typename(t) => t.name.namespace == *dep_id,
        },
        Expression::ArrayIndex { array, index } => {
            used_in_expr(array, list, dep_id) || used_in_expr(index, list, dep_id)
        }
    }
}

fn has(name: &String, list: &Vec<String>) -> bool {
    return list.contains(name);
}

fn used_in_body(body: &Body, list: &Vec<String>, dep_id: &String) -> bool {
    for s in &body.statements {
        match s {
            Statement::VariableDeclaration {
                type_name,
                forms: _,
                values,
            } => {
                if type_name.name.namespace == *dep_id {
                    return true;
                }
                for v in values {
                    match v {
                        Some(e) => {
                            if used_in_expr(&e, list, dep_id) {
                                return true;
                            }
                        }
                        None => {}
                    }
                }
            }
            Statement::If {
                condition,
                body,
                else_body,
            } => {
                if used_in_expr(&condition, list, dep_id) || used_in_body(&body, list, dep_id) {
                    return true;
                }
                match else_body {
                    Some(e) => {
                        if used_in_body(&e, list, dep_id) {
                            return true;
                        }
                    }
                    None => {}
                }
            }
            Statement::For {
                init,
                condition,
                action,
                body,
            } => {
                if used_in_expr(&condition, list, dep_id)
                    || used_in_expr(&action, list, dep_id)
                    || used_in_body(&body, list, dep_id)
                {
                    return true;
                }
                match init {
                    ForInit::Expression(e) => {
                        if used_in_expr(e, list, dep_id) {
                            return true;
                        }
                    }
                    ForInit::LoopCounterDeclaration {
                        type_name,
                        value,
                        form: _,
                    } => {
                        if used_in_expr(&value, list, dep_id) {
                            return true;
                        }
                        if type_name.name.namespace == *dep_id {
                            return true;
                        }
                    }
                }
            }
            Statement::While { condition, body } => {
                if used_in_expr(&condition, list, dep_id) || used_in_body(&body, list, dep_id) {
                    return true;
                }
            }
            Statement::Return { expression } => match expression {
                Some(e) => {
                    if used_in_expr(&e, list, dep_id) {
                        return true;
                    }
                }
                None => {}
            },
            Statement::Switch {
                value,
                cases,
                default,
            } => {
                if used_in_expr(&value, list, dep_id) {
                    return true;
                }
                match default {
                    Some(d) => {
                        if used_in_body(&d, list, dep_id) {
                            return true;
                        }
                    }
                    None => {}
                }
                for c in cases {
                    match &c.value {
                        SwitchCaseValue::Identifier(x) => {
                            if has(&x, list) {
                                return true;
                            }
                        }
                        SwitchCaseValue::Literal(_) => {}
                    }
                }
            }
            Statement::Expression(x) => {
                if used_in_expr(&x, list, dep_id) {
                    return true;
                }
            }
        }
    }
    return false;
}

pub struct Exports {
    pub consts: Vec<EnumItem>,
    pub fns: Vec<FunctionDeclaration>,
    pub types: Vec<Typedef>,
    pub structs: Vec<StructTypedef>,
}

pub fn get_exports(m: &Module) -> Exports {
    let mut exports = Exports {
        consts: Vec::new(),
        fns: Vec::new(),
        types: Vec::new(),
        structs: Vec::new(),
    };
    for e in &m.elements {
        match e {
            ModuleObject::Enum { is_pub, members } => {
                if *is_pub {
                    for member in members {
                        exports.consts.push(member.clone());
                    }
                }
            }
            ModuleObject::Typedef(x) => {
                if x.is_pub {
                    exports.types.push(x.clone());
                }
            }
            ModuleObject::StructTypedef(x) => {
                if x.is_pub {
                    exports.structs.push(x.clone());
                }
            }
            ModuleObject::StructAliasTypedef {
                is_pub,
                struct_name: _,
                type_alias,
            } => {
                if *is_pub {
                    exports.structs.push(StructTypedef {
                        fields: Vec::new(),
                        is_pub: *is_pub,
                        name: type_alias.clone(),
                    })
                }
            }
            ModuleObject::FunctionDeclaration(f) => {
                if f.is_pub {
                    exports.fns.push(f.clone())
                }
            }
            _ => {}
        }
    }
    return exports;
}
