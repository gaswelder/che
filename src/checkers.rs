use crate::nodes::*;

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
        list.push(x.form.alias);
    }
    for x in exports.structs {
        list.push(x.name);
    }
    for obj in &m.elements {
        match obj {
            ModuleObject::CompatMacro(_) => {}
            ModuleObject::Import { .. } => {}
            ModuleObject::Enum(x) => {
                for m in &x.members {
                    match &m.value {
                        Some(e) => {
                            if used_in_expr(e, &list) {
                                return true;
                            }
                        }
                        None => {}
                    }
                }
            }
            ModuleObject::Typedef(Typedef {
                is_pub: _,
                type_name,
                ..
            }) => {
                if has(&type_name.name, &list) {
                    return true;
                }
            }
            ModuleObject::FuncTypedef(FuncTypedef {
                is_pub: _,
                return_type,
                params,
                name: _,
            }) => {
                if has(&return_type.name, &list) {
                    return true;
                }
                for p in &params.forms {
                    if has(&p.type_name.name, &list) {
                        return true;
                    }
                }
            }
            ModuleObject::StructTypedef(StructTypedef { is_pub, name, .. }) => {
                if *is_pub && has(&name, &list) {
                    return true;
                }
            }
            ModuleObject::ModuleVariable(x) => {
                if has(&x.type_name.name, &list) || used_in_expr(&x.value, &list) {
                    return true;
                }
            }
            ModuleObject::FunctionDeclaration(x) => {
                if has(&x.type_name.name, &list) {
                    return true;
                }
                for p in &x.parameters.list {
                    if has(&p.type_name.name, &list) {
                        return true;
                    }
                }
                if used_in_body(&x.body, &list) {
                    return true;
                }
            }
        }
    }
    return false;
}

fn used_in_expr(e: &Expression, list: &Vec<String>) -> bool {
    match e {
        Expression::Literal(_) => false,
        Expression::CompositeLiteral(CompositeLiteral { entries }) => {
            for e in entries {
                match &e.key {
                    Some(expr) => {
                        if used_in_expr(&expr, list) {
                            return true;
                        }
                    }
                    None => {}
                }
                if used_in_expr(&e.value, list) {
                    return true;
                }
            }
            return false;
        }
        Expression::Identifier(x) => has(x, list),
        Expression::BinaryOp { op: _, a, b } => used_in_expr(a, list) || used_in_expr(b, list),
        Expression::PrefixOperator {
            operator: _,
            operand,
        } => used_in_expr(operand, list),
        Expression::PostfixOperator {
            operand,
            operator: _,
        } => used_in_expr(operand, list),
        Expression::Cast { type_name, operand } => {
            has(&type_name.type_name.name, list) || used_in_expr(operand, list)
        }
        Expression::FunctionCall {
            function,
            arguments,
        } => {
            if used_in_expr(function, list) {
                return true;
            }
            for a in arguments {
                if used_in_expr(a, list) {
                    return true;
                }
            }
            return false;
        }
        Expression::Sizeof { argument } => match &**argument {
            SizeofArgument::Expression(e) => used_in_expr(e, list),
            SizeofArgument::Typename(t) => has(&t.name, list),
        },
        Expression::ArrayIndex { array, index } => {
            used_in_expr(array, list) || used_in_expr(index, list)
        }
    }
}

fn has(name: &String, list: &Vec<String>) -> bool {
    return list.contains(name);
}

fn used_in_body(body: &Body, list: &Vec<String>) -> bool {
    for s in &body.statements {
        match s {
            Statement::VariableDeclaration {
                type_name,
                forms: _,
                values,
            } => {
                if has(&type_name.name, list) {
                    return true;
                }
                for v in values {
                    match v {
                        Some(e) => {
                            if used_in_expr(&e, list) {
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
                if used_in_expr(&condition, list) || used_in_body(&body, list) {
                    return true;
                }
                match else_body {
                    Some(e) => {
                        if used_in_body(&e, list) {
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
                if used_in_expr(&condition, list)
                    || used_in_expr(&action, list)
                    || used_in_body(&body, list)
                {
                    return true;
                }
                match init {
                    ForInit::Expression(e) => {
                        if used_in_expr(e, list) {
                            return true;
                        }
                    }
                    ForInit::LoopCounterDeclaration {
                        type_name,
                        value,
                        form: _,
                    } => {
                        if used_in_expr(&value, list) {
                            return true;
                        }
                        if has(&type_name.name, list) {
                            return true;
                        }
                    }
                }
            }
            Statement::While { condition, body } => {
                if used_in_expr(&condition, list) || used_in_body(&body, list) {
                    return true;
                }
            }
            Statement::Return { expression } => match expression {
                Some(e) => {
                    if used_in_expr(&e, list) {
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
                if used_in_expr(&value, list) {
                    return true;
                }
                match default {
                    Some(d) => {
                        if used_in_body(&d, list) {
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
                if used_in_expr(&x, list) {
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
            ModuleObject::Enum(x) => {
                if x.is_pub {
                    for member in &x.members {
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