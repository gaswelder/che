use crate::c::*;
use crate::parser;

pub fn format_module(cm: &CModule) -> String {
    let mut s = String::new();
    for e in &cm.elements {
        s += &match e {
            CModuleObject::ModuleVariable {
                type_name,
                form,
                value,
            } => format!(
                "static {} {} = {};\n",
                format_type(type_name),
                format_form(form),
                format_expression(value)
            ),
            CModuleObject::Typedef {
                type_name, form, ..
            } => format_typedef(&type_name, &form),
            CModuleObject::Macro { name, value } => format!("#{} {}\n", name, value),
            CModuleObject::Include(x) => format!("#include {}\n", x),
            CModuleObject::EnumDefinition { members, .. } => {
                let mut s1 = String::from("enum {\n");
                for (i, member) in members.iter().enumerate() {
                    if i > 0 {
                        s1 += ",\n";
                    }
                    s1 += &format!("\t{}", format_enum_member(&member));
                }
                s1 += "\n};\n";
                s1
            }
            CModuleObject::StructForwardDeclaration(x) => format!("struct {};\n", x),
            CModuleObject::StructDefinition {
                name,
                fields,
                is_pub: _,
            } => format_compat_struct_definition(name, fields),
            CModuleObject::FunctionForwardDeclaration {
                is_static,
                type_name,
                form,
                parameters,
            } => format_function_forward_declaration(*is_static, type_name, form, parameters),
            CModuleObject::FunctionDefinition {
                is_static,
                type_name,
                form,
                parameters,
                body,
            } => {
                let form = format_form(&form);
                let mut s = String::new();
                if *is_static && form != "main" {
                    s += "static ";
                }
                s += &format!(
                    "{} {} {} {}",
                    format_type(&type_name),
                    form,
                    format_compat_function_parameters(&parameters),
                    format_body(&body)
                );
                s
            }
        }
    }
    return s;
}

fn format_union(node: &CUnion) -> String {
    let mut s = String::new();
    for field in &node.fields {
        s += &format!(
            "\t{t} {name};\n",
            t = format_type(&field.type_name),
            name = format_form(&field.form)
        );
    }
    return format!(
        "union {{\n{fields}\n}} {name};\n",
        fields = s,
        name = format_form(&node.form)
    );
}

pub fn format_type(t: &CTypename) -> String {
    return format!("{}{}", if t.is_const { "const " } else { "" }, t.name);
}

pub fn format_form(node: &CForm) -> String {
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

fn format_expression(expr: &CExpression) -> String {
    match expr {
        CExpression::Cast { type_name, operand } => {
            return format!(
                "({})({})",
                format_anonymous_typeform(&type_name),
                format_expression(&operand)
            );
        }
        CExpression::FunctionCall {
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
        CExpression::Literal(x) => format_literal(x),
        CExpression::Identifier(x) => x.clone(),
        CExpression::CompositeLiteral(CCompositeLiteral { entries }) => {
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
        CExpression::Sizeof { argument } => {
            let arg = match &**argument {
                CSizeofArgument::Typename(x) => format_type(&x),
                CSizeofArgument::Expression(x) => format_expression(&x),
            };
            return format!("sizeof({})", arg);
        }
        CExpression::FieldAccess {
            op,
            target,
            field_name,
        } => {
            match is_op(target) {
                Some(targetop) => {
                    if parser::operator_strength(&targetop) < parser::operator_strength(op) {
                        return format!("({}){}{}", format_expression(target), op, field_name);
                    }
                }
                None => {}
            }
            return format!("{}{}{}", format_expression(target), op, field_name);
        }
        CExpression::BinaryOp { op, a, b } => format_binary_op(op, a, b),
        CExpression::PrefixOperator { operator, operand } => {
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
                CExpression::BinaryOp { op, a, b } => {
                    format!("{}({})", operator, format_binary_op(&op, &a, &b))
                }
                CExpression::Cast { type_name, operand } => format!(
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
        CExpression::PostfixOperator { operator, operand } => {
            return format_expression(&operand) + &operator;
        }
        CExpression::ArrayIndex { array, index } => {
            return format!("{}[{}]", format_expression(array), format_expression(index));
        }
    }
}

fn format_anonymous_typeform(node: &CAnonymousTypeform) -> String {
    let mut s = format_type(&node.type_name);
    for op in &node.ops {
        s += &op;
    }
    return s;
}

fn format_anonymous_parameters(params: &CAnonymousParameters) -> String {
    let mut s = String::from("(");
    for (i, form) in params.forms.iter().enumerate() {
        if i > 0 {
            s += ", ";
        }
        s += &format_anonymous_typeform(form);
    }
    if params.ellipsis {
        s += ", ...";
    }
    s += ")";
    return s;
}

fn is_binary_op(a: &CExpression) -> Option<&String> {
    match a {
        CExpression::BinaryOp { op, .. } => Some(op),
        _ => None,
    }
}

fn is_op(e: &CExpression) -> Option<String> {
    match e {
        CExpression::BinaryOp { op, .. } => Some(String::from(op)),
        CExpression::PostfixOperator { .. } => Some(String::from("prefix")),
        CExpression::PrefixOperator { .. } => Some(String::from("prefix")),
        _ => None,
    }
}

fn format_binary_op(op: &String, a: &CExpression, b: &CExpression) -> String {
    // If a is an op weaker than op, wrap it
    let a_formatted = match is_op(a) {
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
    let b_formatted = match is_op(b) {
        Some(k) => {
            if parser::operator_strength(&k) < parser::operator_strength(op) {
                format!("({})", format_expression(b))
            } else {
                format_expression(b)
            }
        }
        None => format_expression(b),
    };
    return format!("{} {} {}", a_formatted, op.clone(), b_formatted);
}

fn indent(text: &str) -> String {
    if text.ends_with("\n") {
        return indent(&text[0..text.len() - 1]) + "\n";
    }
    return String::from("\t") + &text.replace("\n", "\n\t");
}

fn format_body(node: &CBody) -> String {
    let mut s = String::new();
    for statement in &node.statements {
        s += &format_statement(&statement);
        s += "\n";
    }
    return format!("{{\n{}}}\n", indent(&s));
}

fn format_function_forward_declaration(
    is_static: bool,
    type_name: &CTypename,
    form: &CForm,
    parameters: &CompatFunctionParameters,
) -> String {
    let mut s = String::new();
    let form = format_form(form);
    if is_static && form != "main" {
        s += "static ";
    }
    s += &format!(
        "{} {} {};\n",
        format_type(type_name),
        form,
        format_compat_function_parameters(parameters)
    );
    return s;
}

fn format_typeform(x: &CTypeForm) -> String {
    return format!("{} {};\n", format_type(&x.type_name), format_form(&x.form));
}

pub fn format_compat_struct_definition(name: &String, fields: &Vec<CStructItem>) -> String {
    let mut s = String::new();
    for entry in fields {
        match entry {
            CStructItem::Field(x) => {
                s += &format_typeform(x);
            }
            CStructItem::Union(x) => {
                s += &format_union(&x);
            }
        }
    }
    return format!(
        "struct {name} {{\n{contents}\n}};\n",
        name = name,
        contents = indent(&s)
    );
}

fn format_enum_member(node: &CEnumItem) -> String {
    let mut s = node.id.clone();
    if node.value.is_some() {
        s += " = ";
        s += &format_expression(node.value.as_ref().unwrap());
    }
    return s;
}

fn format_for(
    init: &Option<CForInit>,
    condition: &Option<CExpression>,
    action: &Option<CExpression>,
    body: &CBody,
) -> String {
    let init = match init {
        Some(init) => match init {
            CForInit::Expression(x) => format_expression(&x),
            CForInit::LoopCounterDeclaration {
                type_name,
                form,
                value,
            } => format!(
                "{} {} = {}",
                format_type(type_name),
                format_form(form),
                format_expression(value)
            ),
        },
        None => String::from(""),
    };
    let condition = match condition {
        Some(c) => format_expression(c),
        None => String::from(""),
    };
    let action = match action {
        Some(a) => format_expression(a),
        None => String::from(""),
    };
    return format!(
        "for ({}; {}; {}) {}",
        init,
        condition,
        action,
        format_body(body)
    );
}

fn format_compat_function_parameters(parameters: &CompatFunctionParameters) -> String {
    let mut s = String::from("(");
    for (i, parameter) in parameters.list.iter().enumerate() {
        if i > 0 {
            s += ", ";
        }
        s += &format_type(&parameter.type_name);
        s += " ";
        s += &format_form(&parameter.form);
    }
    if parameters.variadic {
        s += ", ...";
    }
    if parameters.list.len() == 0 {
        s += "void";
    }
    s += ")";
    return s;
}

fn format_literal(node: &CLiteral) -> String {
    match node {
        CLiteral::Char(val) => format!("\'{}\'", val),
        CLiteral::String(val) => {
            return val
                .split("\n")
                .map(|line| format!("\"{}\"", line))
                .collect::<Vec<String>>()
                .join("\"\\n\"\n");
        }
        CLiteral::Number(val) => format!("{}", val),
        CLiteral::Null => String::from("NULL"),
    }
}

fn format_statement(node: &CStatement) -> String {
    match node {
        CStatement::Block { statements } => format_body(&CBody {
            statements: statements.clone(),
        }),
        CStatement::Break => format!("break;"),
        CStatement::Continue => format!("continue;"),
        CStatement::VariableDeclaration {
            type_name,
            forms,
            values,
        } => {
            let mut s = String::from(format_type(&type_name)) + " ";
            for (i, form) in forms.iter().enumerate() {
                let value = &values[i];
                if i > 0 {
                    s += ", ";
                }
                match value {
                    Some(x) => {
                        s += &format!("{} = {}", &format_form(&form), &format_expression(x));
                    }
                    None => {
                        s += &format!("{}", format_form(&form));
                    }
                }
            }
            return s + ";";
        }
        CStatement::If {
            condition,
            body,
            else_body,
        } => {
            let mut s = format!(
                "if ({cond}) {body}",
                cond = format_expression(condition),
                body = format_body(body)
            );
            if else_body.is_some() {
                s += &format!(" else {}", format_body(else_body.as_ref().unwrap()));
            }
            return s;
        }
        CStatement::For {
            init,
            condition,
            action,
            body,
        } => format_for(init, condition, action, body),
        CStatement::While { condition, body } => {
            return format!(
                "while ({}) {}",
                format_expression(condition),
                format_body(body)
            );
        }
        CStatement::Return { expression } => {
            return match expression {
                None => String::from("return;"),
                Some(x) => format!("return {}", format_expression(&x)),
            } + ";";
        }
        CStatement::Switch {
            value,
            cases,
            default,
        } => format_switch(value, cases, default),
        CStatement::Expression(x) => format_expression(&x) + ";",
    }
}

fn format_switch(value: &CExpression, cases: &Vec<CSwitchCase>, default: &Option<CBody>) -> String {
    let mut s = String::new();
    for case in cases {
        for v in &case.values {
            let valstring = match v {
                CSwitchCaseValue::Identifier(x) => x.clone(),
                CSwitchCaseValue::Literal(x) => format_literal(&x),
            };
            s += &format!("case {}:\n", valstring);
        }
        s += &format!("{{\n");
        for statement in &case.body.statements {
            s += &format_statement(&statement);
            s += ";\n";
        }
        s += "break; }\n";
    }
    match default {
        None => {}
        Some(b) => {
            s += "default: {\n";
            for statement in &b.statements {
                s += &format_statement(statement);
                s += ";\n";
            }
            s += "}\n";
        }
    }
    return format!(
        "switch ({}) {{\n{}\n}}",
        format_expression(value),
        indent(&s)
    );
}

pub fn format_typedef(t: &CTypename, form: &CTypedefForm) -> String {
    let mut formstr = form.stars.clone() + &form.alias;
    if form.params.is_some() {
        formstr += &format_anonymous_parameters(&form.params.as_ref().unwrap());
    }
    if form.size > 0 {
        formstr += &format!("[{}]", form.size);
    }
    let t = format_type(t);
    return format!("typedef {} {};\n", t, formstr);
}
