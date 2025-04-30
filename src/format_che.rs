use crate::nodes;
use crate::nodes::*;
use crate::parser;

pub fn format_expression(expr: &Expression) -> String {
    match expr {
        Expression::FieldAccess(x) => {
            let op = &x.op;
            let target = &x.target;
            let field_name = &x.field_name;
            return format!("{}{}{}", format_expression(target), op, field_name.name);
        }
        Expression::Cast(x) => {
            let type_name = &x.type_name;
            let operand = &x.operand;
            return format!(
                "({})({})",
                fmt_anonymous_typeform(&type_name),
                format_expression(&operand)
            );
        }
        Expression::NsName(n) => format!("{}.{}", &n.namespace, &n.name),
        Expression::FunctionCall(x) => {
            let arguments = &x.args;
            let function = &x.func;
            let mut s1 = String::from("(");
            for (i, argument) in arguments.iter().enumerate() {
                if i > 0 {
                    s1 += ", ";
                }
                s1 += &format_expression(&argument);
            }
            s1 += ")";
            return format!("{}{}", format_expression(&function), s1);
        }
        Expression::Literal(x) => fmt_literal(x),
        Expression::Ident(x) => x.name.clone(),
        Expression::CompositeLiteral(CompositeLiteral { entries }) => {
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
                let v = format_expression(&e.value);
                match &e.key {
                    Some(expr) => {
                        let k = format_expression(expr);
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
        Expression::Sizeof(x) => {
            let argument = &x.argument;
            let arg = match &**argument {
                SizeofArg::Typename(x) => fmt_typename(&x),
                SizeofArg::Expr(x) => format_expression(&x),
            };
            return format!("sizeof({})", arg);
        }
        Expression::BinaryOp(x) => fmt_binop(&x),
        Expression::PrefixOperator(x) => {
            let operand = &x.operand;
            let operator = &x.operator;
            let expr = &**operand;
            match is_binary_op(expr) {
                Some(op) => {
                    if parser::operator_strength("prefix") > parser::operator_strength(op) {
                        return format!("{}({})", operator, format_expression(expr));
                    }
                    return format!("{}{}", operator, format_expression(expr));
                }
                None => {}
            }
            return match expr {
                Expression::BinaryOp(x) => {
                    format!("{}({})", operator, fmt_binop(&x))
                }
                Expression::Cast(x) => {
                    let type_name = &x.type_name;
                    let operand = &x.operand;
                    format!(
                        "{}{}",
                        operator,
                        format!(
                            "({})({})",
                            fmt_anonymous_typeform(&type_name),
                            format_expression(&operand)
                        )
                    )
                }
                _ => format!("{}{}", operator, format_expression(&operand)),
            };
        }
        Expression::PostfixOperator(x) => {
            let operand = &x.operand;
            let operator = &x.operator;
            return format_expression(&operand) + &operator;
        }
        Expression::ArrayIndex(x) => {
            return format!(
                "{}[{}]",
                format_expression(&x.array),
                format_expression(&x.index)
            );
        }
    }
}

pub fn fmt_typename(t: &Typename) -> String {
    let name = if t.name.namespace != "" {
        format!("{}.{}", t.name.namespace, t.name.name)
    } else {
        t.name.name.clone()
    };
    return format!("{}{}", if t.is_const { "const " } else { "" }, name);
}

pub fn fmt_form(node: &Form) -> String {
    let mut s = String::new();
    for _ in 0..node.hops {
        s += "*";
    }
    s += &node.name;
    for expr in &node.indexes {
        match expr {
            Some(e) => s += &format!("[{}]", format_expression(&e)),
            None => s += "[]",
        }
    }
    return s;
}

fn fmt_anonymous_typeform(node: &AnonymousTypeform) -> String {
    let mut s = fmt_typename(&node.type_name);
    for op in &node.ops {
        s += &op;
    }
    return s;
}

fn fmt_binop(x: &nodes::BinaryOp) -> String {
    let op = &x.op;
    let a = &x.a;
    let b = &x.b;

    // If a is an op weaker than op, wrap it
    let af = match is_op(a) {
        Some(k) => {
            if parser::operator_strength(&k) < parser::operator_strength(op) {
                format!("({})", format_expression(a))
            } else {
                format_expression(a)
            }
        }
        None => format_expression(a),
    };
    // If b is an op weaker than op, wrap it
    let bf = match is_op(b) {
        Some(k) => {
            if parser::operator_strength(&k) < parser::operator_strength(op) {
                format!("({})", format_expression(b))
            } else {
                format_expression(b)
            }
        }
        None => format_expression(b),
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
