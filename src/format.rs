use crate::nodes::*;
use crate::nodes_c::*;
use crate::parser;

pub fn format_module(node: &CModule) -> String {
    let mut s = String::new();
    for e in &node.elements {
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
            CModuleObject::FuncTypedef {
                return_type,
                name,
                params,
                is_pub: _,
            } => format!(
                "typedef {} (*{}){};\n",
                format_type(return_type),
                name,
                format_anonymous_parameters(params)
            ),
            CModuleObject::CompatMacro(x) => format_compat_macro(&x),
            CModuleObject::CompatInclude(x) => format!("#include {}\n", x),
            CModuleObject::Enum { members, .. } => {
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
            CModuleObject::CompatStructForwardDeclaration(x) => format!("struct {};\n", x),
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
            CModuleObject::CompatFunctionDeclaration(x) => format_compat_function_declaration(&x),
            CModuleObject::CompatSplit { text } => format!("\n\n{}\n", text),
        }
    }
    return s;
}

fn format_union(node: &Union) -> String {
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

fn format_expression(expr: &Expression) -> String {
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

fn format_anonymous_typeform(node: &AnonymousTypeform) -> String {
    let mut s = format_type(&node.type_name);
    for op in &node.ops {
        s += &op;
    }
    return s;
}

fn format_anonymous_parameters(params: &AnonymousParameters) -> String {
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

fn is_binary_op(a: &Expression) -> Option<&String> {
    match a {
        Expression::BinaryOp { op, .. } => Some(op),
        _ => None,
    }
}

fn is_op(e: &Expression) -> Option<String> {
    match e {
        Expression::BinaryOp { op, .. } => Some(String::from(op)),
        Expression::PostfixOperator { .. } => Some(String::from("prefix")),
        Expression::PrefixOperator { .. } => Some(String::from("prefix")),
        _ => None,
    }
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

fn indent(text: &str) -> String {
    if text.ends_with("\n") {
        return indent(&text[0..text.len() - 1]) + "\n";
    }
    return String::from("\t") + &text.replace("\n", "\n\t");
}

fn format_body(node: &Body) -> String {
    let mut s = String::new();
    for statement in &node.statements {
        s += &format_statement(&statement);
        s += "\n";
    }
    return format!("{{\n{}}}\n", indent(&s));
}

fn format_compat_function_declaration(node: &CompatFunctionDeclaration) -> String {
    let form = format_form(&node.form);
    let mut s = String::new();
    if node.is_static && form != "main" {
        s += "static ";
    }

    s += &format!(
        "{} {} {} {}",
        format_type(&node.type_name),
        form,
        format_compat_function_parameters(&node.parameters),
        format_body(&node.body)
    );
    return s;
}

fn format_function_forward_declaration(
    is_static: bool,
    type_name: &Typename,
    form: &Form,
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

pub fn format_compat_struct_definition(name: &String, fields: &Vec<CompatStructEntry>) -> String {
    let mut s = String::new();
    for entry in fields {
        match entry {
            CompatStructEntry::CompatStructField { type_name, form } => {
                s += &format!("{} {};\n", format_type(&type_name), format_form(&form));
            }
            CompatStructEntry::Union(x) => {
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

fn format_enum_member(node: &EnumItem) -> String {
    let mut s = node.id.clone();
    if node.value.is_some() {
        s += " = ";
        s += &format_expression(node.value.as_ref().unwrap());
    }
    return s;
}

fn format_for(init: &ForInit, condition: &Expression, action: &Expression, body: &Body) -> String {
    let init = match init {
        ForInit::Expression(x) => format_expression(&x),
        ForInit::LoopCounterDeclaration {
            type_name,
            form,
            value,
        } => format!(
            "{} {} = {}",
            format_type(type_name),
            format_form(form),
            format_expression(value)
        ),
    };
    return format!(
        "for ({}; {}; {}) {}",
        init,
        format_expression(condition),
        format_expression(action),
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
    s += ")";
    return s;
}

fn format_literal(node: &Literal) -> String {
    match node.type_name.as_str() {
        "string" => format!("\"{}\"", node.value),
        "char" => format!("\'{}\'", node.value),
        _ => node.value.clone(),
    }
}

fn format_compat_macro(node: &CompatMacro) -> String {
    format!("#{} {}\n", node.name, node.value)
}

fn format_statement(node: &Statement) -> String {
    match node {
        Statement::VariableDeclaration {
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
        Statement::If {
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
        Statement::For {
            init,
            condition,
            action,
            body,
        } => format_for(init, condition, action, body),
        Statement::While { condition, body } => {
            return format!(
                "while ({}) {}",
                format_expression(condition),
                format_body(body)
            );
        }
        Statement::Return { expression } => {
            return match expression {
                None => String::from("return;"),
                Some(x) => format!("return {}", format_expression(&x)),
            } + ";";
        }
        Statement::Switch {
            value,
            cases,
            default,
        } => format_switch(value, cases, default),
        Statement::Expression(x) => format_expression(&x) + ";",
    }
}

fn format_switch(value: &Expression, cases: &Vec<SwitchCase>, default: &Option<Body>) -> String {
    let mut s = String::new();
    for case in cases {
        let val = match &case.value {
            SwitchCaseValue::Identifier(x) => x.clone(),
            SwitchCaseValue::Literal(x) => format_literal(&x),
        };
        s += &format!("case {}: {{\n", val);
        for statement in &case.body.statements {
            s += &format_statement(&statement);
            s += ";\n";
        }
        s += "}\n";
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

pub fn format_typedef(type_name: &Typename, form: &TypedefForm) -> String {
    let mut formstr = form.stars.clone() + &form.alias;
    if form.params.is_some() {
        formstr += &format_anonymous_parameters(&form.params.as_ref().unwrap());
    }
    if form.size > 0 {
        formstr += &format!("[{}]", form.size);
    }
    let t = format_type(type_name);
    return format!("typedef {} {};\n", t, formstr);
}
