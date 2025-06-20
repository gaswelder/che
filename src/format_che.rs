use crate::nodes;
use crate::nodes::*;
use crate::parser;

pub fn fmt_field_access(x: &FieldAccess) -> String {
    format!("{}{}{}", fmt_expr(&x.target), &x.op, &x.field_name)
}

pub fn fmt_expr(expr: &Expr) -> String {
    match expr {
        Expr::FieldAccess(x) => fmt_field_access(x),
        Expr::Cast(x) => {
            let type_name = &x.typeform;
            let operand = &x.operand;
            return format!(
                "({})({})",
                fmt_base_typeform(&type_name),
                fmt_expr(&operand)
            );
        }
        Expr::NsName(n) => {
            if n.ns == "" {
                format!("{}", &n.name)
            } else {
                format!("{}.{}", &n.ns, &n.name)
            }
        }
        Expr::Call(x) => {
            let arguments = &x.args;
            let function = &x.func;
            let mut s1 = String::from("(");
            for (i, argument) in arguments.iter().enumerate() {
                if i > 0 {
                    s1 += ", ";
                }
                s1 += &fmt_expr(&argument);
            }
            s1 += ")";
            return format!("{}{}", fmt_expr(&function), s1);
        }
        Expr::Literal(x) => fmt_literal(x),
        Expr::CompositeLiteral(CompLiteral { entries }) => {
            if entries.len() == 0 {
                // Print {0} to avoid "ISO C forbids empty initializers".
                return String::from("{0}");
            }
            let mut s = String::from("{\n");
            for (i, e) in entries.iter().enumerate() {
                if i > 0 {
                    s += ",\n";
                }
                s += "\t";
                let v = fmt_expr(&e.value);
                match &e.key {
                    Some(expr) => {
                        let k = fmt_expr(expr);
                        if e.is_index {
                            s += &format!("[{}] = {}", k, v)
                        } else {
                            s += &format!(".{} = {}", k, v)
                        }
                    }
                    None => s += &v,
                }
            }
            s += "\n}";
            return s;
        }
        Expr::Sizeof(x) => {
            let argument = &x.arg;
            let arg = match &**argument {
                SizeofArg::Typename(x) => fmt_typename(&x),
                SizeofArg::Expr(x) => fmt_expr(&x),
            };
            return format!("sizeof({})", arg);
        }
        Expr::BinaryOp(x) => fmt_binop(&x),
        Expr::PrefixOperator(x) => {
            let operand = &x.operand;
            let operator = &x.operator;
            let expr = &**operand;
            match is_binary_op(expr) {
                Some(op) => {
                    if parser::operator_strength("prefix") > parser::operator_strength(op) {
                        return format!("{}({})", operator, fmt_expr(expr));
                    }
                    return format!("{}{}", operator, fmt_expr(expr));
                }
                None => {}
            }
            return match expr {
                Expr::BinaryOp(x) => {
                    format!("{}({})", operator, fmt_binop(&x))
                }
                Expr::Cast(x) => {
                    let type_name = &x.typeform;
                    let operand = &x.operand;
                    format!(
                        "{}{}",
                        operator,
                        format!(
                            "({})({})",
                            fmt_base_typeform(&type_name),
                            fmt_expr(&operand)
                        )
                    )
                }
                _ => format!("{}{}", operator, fmt_expr(&operand)),
            };
        }
        Expr::PostfixOperator(x) => {
            let operand = &x.operand;
            let operator = &x.operator;
            return fmt_expr(&operand) + &operator;
        }
        Expr::ArrIndex(x) => {
            return format!("{}[{}]", fmt_expr(&x.array), fmt_expr(&x.index));
        }
    }
}

pub fn fmt_typename(t: &Typename) -> String {
    let name = if t.name.ns != "" {
        format!("{}.{}", t.name.ns, t.name.name)
    } else {
        t.name.name.clone()
    };
    return format!("{}{}", if t.is_const { "const " } else { "" }, name);
}

pub fn fmt_form(x: &Form) -> String {
    let mut s = "*".repeat(x.hops) + &x.name;
    for expr in &x.indexes {
        match expr {
            Some(e) => s += &format!("[{}]", fmt_expr(&e)),
            None => s += "[]",
        }
    }
    s
}

fn fmt_base_typeform(x: &BareTypeform) -> String {
    format!("{}{}", fmt_typename(&x.typename), "*".repeat(x.hops))
}

pub fn fmt_binop(x: &nodes::BinaryOp) -> String {
    let op = &x.op;
    let a = &x.a;
    let b = &x.b;

    // If a is an op weaker than op, wrap it
    let af = match is_op(a) {
        Some(k) => {
            if parser::operator_strength(&k) < parser::operator_strength(op) {
                format!("({})", fmt_expr(a))
            } else {
                fmt_expr(a)
            }
        }
        None => fmt_expr(a),
    };
    // If b is an op weaker than op, wrap it
    let bf = match is_op(b) {
        Some(k) => {
            if parser::operator_strength(&k) < parser::operator_strength(op) {
                format!("({})", fmt_expr(b))
            } else {
                fmt_expr(b)
            }
        }
        None => fmt_expr(b),
    };
    let parts = vec![af, op.clone(), bf];
    let glue = if op == "." || op == "->" { "" } else { " " };
    return parts.join(glue);
}

fn fmt_literal(node: &Literal) -> String {
    match node {
        Literal::Char(val) => format!("\'{}\'", val),
        Literal::String(val) => format!("\"{}\"", val),
        Literal::Number(val) => format!("{}", val),
        Literal::Null => String::from("NULL"),
    }
}
