use crate::{exports::get_exports, nodes::*};

// Tells whether any of the exports of dep are used in m.
pub fn depused(m: &Module, dep: &Module, depns: &String) -> bool {
    let exports = get_exports(dep);
    let mut list: Vec<String> = Vec::new();
    for x in exports.consts {
        list.push(x.id.name);
    }
    for x in exports.fns {
        list.push(x.form.name);
    }
    for x in exports.types {
        list.push(x);
    }
    // for x in exports.structs {
    //     list.push(x.name);
    // }
    for obj in &m.elements {
        match obj {
            ModuleObject::Macro { .. } => {}
            // ModuleObject::Import { .. } => {}
            ModuleObject::Enum { is_pub: _, members } => {
                for m in members {
                    match &m.value {
                        Some(e) => {
                            if used_in_expr(e, &list, &depns) {
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
                if type_name.name.namespace == *depns {
                    return true;
                }
            }
            ModuleObject::StructTypedef(StructTypedef {
                pos: _,
                is_pub,
                name,
                fields,
            }) => {
                if *is_pub && has(&name.name, &list) {
                    return true;
                }
                for f in fields {
                    match f {
                        StructEntry::Plain(e) => {
                            if e.type_name.name.namespace == *depns {
                                return true;
                            }
                        }
                        StructEntry::Union(_) => {}
                    }
                }
            }
            ModuleObject::ModuleVariable {
                form: _,
                type_name,
                value,
            } => {
                if type_name.name.namespace == *depns || used_in_expr(&value, &list, &depns) {
                    return true;
                }
            }
            ModuleObject::FunctionDeclaration(x) => {
                if x.type_name.name.namespace == *depns {
                    return true;
                }
                for p in &x.parameters.list {
                    if p.type_name.name.namespace == *depns {
                        return true;
                    }
                }
                if used_in_body(&x.body, &list, &depns) {
                    return true;
                }
            }
        }
    }
    return false;
}

fn used_in_expr(e: &Expression, list: &Vec<String>, depns: &String) -> bool {
    match e {
        Expression::Literal(_) => false,
        Expression::NsName(n) => n.namespace == *depns,
        Expression::CompositeLiteral(CompositeLiteral { entries }) => {
            for e in entries {
                match &e.key {
                    Some(expr) => {
                        if used_in_expr(&expr, list, depns) {
                            return true;
                        }
                    }
                    None => {}
                }
                if used_in_expr(&e.value, list, depns) {
                    return true;
                }
            }
            return false;
        }
        Expression::Identifier(x) => has(&x.name, list),
        Expression::BinaryOp { op: _, a, b } => {
            used_in_expr(a, list, depns) || used_in_expr(b, list, depns)
        }
        Expression::PrefixOperator {
            operator: _,
            operand,
        } => used_in_expr(operand, list, depns),
        Expression::PostfixOperator {
            operand,
            operator: _,
        } => used_in_expr(operand, list, depns),
        Expression::Cast { type_name, operand } => {
            type_name.type_name.name.namespace == *depns || used_in_expr(operand, list, depns)
        }
        Expression::FunctionCall {
            function,
            arguments,
        } => {
            if used_in_expr(function, list, depns) {
                return true;
            }
            for a in arguments {
                if used_in_expr(a, list, depns) {
                    return true;
                }
            }
            return false;
        }
        Expression::Sizeof { argument } => match &**argument {
            SizeofArgument::Expression(e) => used_in_expr(e, list, depns),
            SizeofArgument::Typename(t) => t.name.namespace == *depns,
        },
        Expression::ArrayIndex { array, index } => {
            used_in_expr(array, list, depns) || used_in_expr(index, list, depns)
        }
    }
}

fn has(name: &String, list: &Vec<String>) -> bool {
    return list.contains(name);
}

fn used_in_body(body: &Body, list: &Vec<String>, depns: &String) -> bool {
    for s in &body.statements {
        match s {
            Statement::Panic { arguments, pos: _ } => {
                for arg in arguments {
                    if used_in_expr(arg, list, depns) {
                        return true;
                    }
                }
            }
            Statement::VariableDeclaration {
                type_name,
                forms: _,
                values,
            } => {
                if type_name.name.namespace == *depns {
                    return true;
                }
                for v in values {
                    match v {
                        Some(e) => {
                            if used_in_expr(&e, list, depns) {
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
                if used_in_expr(&condition, list, depns) || used_in_body(&body, list, depns) {
                    return true;
                }
                match else_body {
                    Some(e) => {
                        if used_in_body(&e, list, depns) {
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
                if used_in_expr(&condition, list, depns)
                    || used_in_expr(&action, list, depns)
                    || used_in_body(&body, list, depns)
                {
                    return true;
                }
                match init {
                    ForInit::Expression(e) => {
                        if used_in_expr(e, list, depns) {
                            return true;
                        }
                    }
                    ForInit::LoopCounterDeclaration {
                        type_name,
                        value,
                        form: _,
                    } => {
                        if used_in_expr(&value, list, depns) {
                            return true;
                        }
                        if type_name.name.namespace == *depns {
                            return true;
                        }
                    }
                }
            }
            Statement::While { condition, body } => {
                if used_in_expr(&condition, list, depns) || used_in_body(&body, list, depns) {
                    return true;
                }
            }
            Statement::Return { expression } => match expression {
                Some(e) => {
                    if used_in_expr(&e, list, depns) {
                        return true;
                    }
                }
                None => {}
            },
            Statement::Switch {
                value,
                cases,
                default_case: default,
            } => {
                if used_in_expr(&value, list, depns) {
                    return true;
                }
                match default {
                    Some(d) => {
                        if used_in_body(&d, list, depns) {
                            return true;
                        }
                    }
                    None => {}
                }
                for c in cases {
                    for v in &c.values {
                        match v {
                            SwitchCaseValue::Identifier(x) => {
                                if has(&x.name, list) {
                                    return true;
                                }
                            }
                            SwitchCaseValue::Literal(_) => {}
                        }
                    }
                    if used_in_body(&c.body, list, depns) {
                        return true;
                    }
                }
            }
            Statement::Expression(x) => {
                if used_in_expr(&x, list, depns) {
                    return true;
                }
            }
        }
    }
    return false;
}
