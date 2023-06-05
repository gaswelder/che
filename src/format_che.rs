use crate::nodes::*;
use crate::parser;

pub fn format_expression(expr: &Expression) -> String {
    match expr {
        Expression::Cast { type_name, operand } => {
            return format!(
                "({})({})",
                format_anonymous_typeform(&type_name),
                format_expression(&operand)
            );
        }
        Expression::FunctionCall {
            function,
            arguments,
        } => {
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
        Expression::Literal(x) => format_literal(x),
        Expression::Identifier(x) => x.clone(),
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
        Expression::Sizeof { argument } => {
            let arg = match &**argument {
                SizeofArgument::Typename(x) => format_type(&x),
                SizeofArgument::Expression(x) => format_expression(&x),
            };
            return format!("sizeof({})", arg);
        }
        Expression::BinaryOp { op, a, b } => format_binary_op(op, a, b),
        Expression::PrefixOperator { operator, operand } => {
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
                Expression::BinaryOp { op, a, b } => {
                    format!("{}({})", operator, format_binary_op(&op, &a, &b))
                }
                Expression::Cast { type_name, operand } => format!(
                    "{}{}",
                    operator,
                    format!(
                        "({})({})",
                        format_anonymous_typeform(&type_name),
                        format_expression(&operand)
                    )
                ),
                _ => format!("{}{}", operator, format_expression(&operand)),
            };
        }
        Expression::PostfixOperator { operator, operand } => {
            return format_expression(&operand) + &operator;
        }
        Expression::ArrayIndex { array, index } => {
            return format!("{}[{}]", format_expression(array), format_expression(index));
        }
    }
}

pub fn format_type(t: &Typename) -> String {
    return format!("{}{}", if t.is_const { "const " } else { "" }, t.name);
}

pub fn format_form(node: &Form) -> String {
    let mut s = String::new();
    s += &node.stars;
    s += &node.name;
    for expr in &node.indexes {
        match expr {
            Some(e) => s += &format!("[{}]", format_expression(&e)),
            None => s += "[]",
        }
    }
    return s;
}

fn format_anonymous_typeform(node: &AnonymousTypeform) -> String {
    let mut s = format_type(&node.type_name);
    for op in &node.ops {
        s += &op;
    }
    return s;
}

fn format_binary_op(op: &String, a: &Expression, b: &Expression) -> String {
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

fn is_op(e: &Expression) -> Option<String> {
    match e {
        Expression::BinaryOp { op, .. } => Some(String::from(op)),
        Expression::PostfixOperator { .. } => Some(String::from("prefix")),
        Expression::PrefixOperator { .. } => Some(String::from("prefix")),
        _ => None,
    }
}

fn is_binary_op(a: &Expression) -> Option<&String> {
    match a {
        Expression::BinaryOp { op, .. } => Some(op),
        _ => None,
    }
}

fn format_literal(node: &Literal) -> String {
    match node.type_name.as_str() {
        "string" => format!("\"{}\"", node.value),
        "char" => format!("\'{}\'", node.value),
        _ => node.value.clone(),
    }
}
