use crate::{
    buf::Pos,
    nodes::{Body, Expression, Statement, Typename},
};

fn istrue(e: &Expression) -> bool {
    return match e {
        Expression::Identifier(x) => x.name == "true",
        _ => false,
    };
}

pub fn isvoid(t: &Typename) -> bool {
    return t.name.namespace == "" && t.name.name == "void";
}

pub fn body_returns(b: &Body) -> bool {
    let last = &b.statements[b.statements.len() - 1];
    return match last {
        Statement::Return { .. } => true,
        Statement::Panic { .. } => true,
        Statement::If {
            condition: _,
            body,
            else_body: _,
        } => body_returns(&body),
        Statement::Switch {
            value: _,
            cases,
            default_case,
        } => {
            if default_case.is_none() || !body_returns(default_case.as_ref().unwrap()) {
                return false;
            }
            for c in cases {
                if !body_returns(&c.body) {
                    return false;
                }
            }
            return true;
        }
        Statement::While { condition, body } => istrue(condition) || body_returns(&body),
        _ => false,
    };
}

pub fn has_function_call(e: &Expression) -> bool {
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

pub fn expression_pos(e: &Expression) -> Pos {
    let todopos = Pos { line: 0, col: 0 };
    match e {
        Expression::ArrayIndex { array, index: _ } => expression_pos(array),
        Expression::BinaryOp { op: _, a, b: _ } => expression_pos(a),
        Expression::FieldAccess {
            op: _,
            target,
            field_name: _,
        } => expression_pos(target),
        Expression::Cast {
            type_name: _,
            operand: _,
        } => todopos,
        Expression::CompositeLiteral(_) => todopos,
        Expression::FunctionCall {
            function,
            arguments: _,
        } => expression_pos(function),
        Expression::Identifier(x) => x.pos.clone(),
        Expression::Literal(_) => todopos, // x.pos.clone(),
        Expression::NsName(x) => x.pos.clone(),
        Expression::PostfixOperator {
            operator: _,
            operand,
        } => expression_pos(operand),
        Expression::PrefixOperator {
            operator: _,
            operand,
        } => expression_pos(operand),
        Expression::Sizeof { argument: _ } => todopos,
    }
}
