use crate::{buf::Pos, nodes::*};

pub fn body_returns(b: &Body) -> bool {
    let n = b.statements.len();
    if n == 0 {
        return false;
    }
    let last = &b.statements[n - 1];
    return match last {
        Statement::Return { .. } => true,
        Statement::If(x) => body_returns(&x.body),
        Statement::Switch(x) => {
            let default_case = &x.default_case;
            let cases = &x.cases;
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
        Statement::While(x) => is_ident(&x.cond, "true") || body_returns(&x.body),
        Statement::Expression(x) => match x {
            Expr::Call(function_call) => {
                if is_ident(&function_call.func, "panic") {
                    return true;
                }
                return false;
            }
            _ => false,
        },
        _ => false,
    };
}

// // Returns true if the given expression contains a function call.
// // This is simply to check that module variable initializations are
// // compile-time expressions.
// pub fn has_function_call(e: &Expression) -> bool {
//     return match e {
//         Expression::FunctionCall(_) => true,
//         Expression::CompositeLiteral(_) => false,
//         Expression::Identifier(_) => false,
//         Expression::Literal(_) => false,
//         Expression::NsName(_) => false,
//         Expression::Sizeof(_) => false,
//         Expression::ArrayIndex(x) => has_function_call(&x.array) || has_function_call(&x.index),
//         Expression::BinaryOp(x) => has_function_call(&x.a) || has_function_call(&x.b),
//         Expression::FieldAccess(x) => has_function_call(&x.target),
//         Expression::Cast(x) => has_function_call(&x.operand),
//         Expression::PostfixOperator(x) => has_function_call(&x.operand),
//         Expression::PrefixOperator(x) => has_function_call(&x.operand),
//     };
// }

// Returns source position of the given expression
// for compilation error reports.
pub fn expression_pos(e: &Expr) -> Pos {
    // Not all nodes have source position yet, so traverse the expression
    // and find any node inside with a position field.
    let todopos = Pos { line: 0, col: 0 };
    match e {
        Expr::ArrIndex(x) => expression_pos(&x.array),
        Expr::BinaryOp(x) => expression_pos(&x.a),
        Expr::FieldAccess(x) => expression_pos(&x.target),
        Expr::Cast(_) => todopos,
        Expr::CompositeLiteral(_) => todopos,
        Expr::Call(x) => expression_pos(&x.func),
        Expr::Literal(_) => todopos, // x.pos.clone(),
        Expr::NsName(x) => x.pos.clone(),
        Expr::PostfixOperator(x) => expression_pos(&x.operand),
        Expr::PrefixOperator(x) => expression_pos(&x.operand),
        Expr::Sizeof(_) => todopos,
    }
}
