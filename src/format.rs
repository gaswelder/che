use crate::nodes::*;
use crate::parser;

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

pub fn format_type(node: &Type) -> String {
    let mut s = String::new();
    if node.is_const {
        s += "const ";
    }
    s += &node.type_name;
    return s;
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
        Expression::StructLiteral { members } => {
            let mut s = String::from("{\n");
            for member in members {
                s += &format!(
                    "\t.{} = {},\n",
                    member.name,
                    format_expression(&member.value)
                );
            }
            s += "}\n";
            return s;
        }
        Expression::ArrayLiteral(x) => format_array_literal(x),
        Expression::Sizeof { argument } => {
            let arg = match &**argument {
                SizeofArgument::Type(x) => format_type(&x),
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

fn format_anonymous_parameters(node: &AnonymousParameters) -> String {
    let mut s = String::from("(");
    for (i, form) in node.forms.iter().enumerate() {
        if i > 0 {
            s += ", ";
        }
        s += &format_anonymous_typeform(form);
    }
    s += ")";
    return s;
}

fn format_array_literal(node: &ArrayLiteral) -> String {
    let mut s = String::from("{");
    if !node.values.is_empty() {
        for (i, entry) in node.values.iter().enumerate() {
            if i > 0 {
                s += ", ";
            }
            s += &match &entry.index {
                ArrayLiteralKey::None => String::from(""),
                ArrayLiteralKey::Identifier(x) => format!("[{}] = ", x),
                ArrayLiteralKey::Literal(x) => format!("[{}] = ", format_literal(&x)),
            };
            s += &match &entry.value {
                ArrayLiteralValue::ArrayLiteral(x) => format_array_literal(&x),
                ArrayLiteralValue::Identifier(x) => x.clone(),
                ArrayLiteralValue::Literal(x) => format_literal(&x),
            };
        }
    } else {
        s += "0";
    }
    s += "}";
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

fn format_compat_function_forward_declaration(node: &CompatFunctionForwardDeclaration) -> String {
    let mut s = String::new();
    let form = format_form(&node.form);
    if node.is_static && form != "main" {
        s += "static ";
    }
    s += &format!(
        "{} {} {};\n",
        format_type(&node.type_name),
        form,
        format_compat_function_parameters(&node.parameters)
    );
    return s;
}

pub fn format_compat_struct_definition(node: &CompatStructDefinition) -> String {
    let mut s = String::new();
    for entry in &node.fields {
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
        name = node.name,
        contents = indent(&s)
    );
}

fn format_anonymous_struct(node: &AnonymousStruct) -> String {
    let mut s = String::new();
    for fieldlist in &node.entries {
        s += &match fieldlist {
            StructEntry::StructFieldlist(x) => format_struct_fieldlist(&x),
            StructEntry::Union(x) => format_union(&x),
        };
        s += "\n";
    }
    return format!("{{\n{}}}", indent(&s));
}

fn format_enum_member(node: &EnumMember) -> String {
    let mut s = node.id.clone();
    if node.value.is_some() {
        s += " = ";
        s += &format_expression(node.value.as_ref().unwrap());
    }
    return s;
}

fn format_for(node: &For) -> String {
    let init = match &node.init {
        ForInit::Expression(x) => format_expression(&x),
        ForInit::LoopCounterDeclaration(x) => format!(
            "{} {} = {}",
            format_type(&x.type_name),
            format_form(&x.form),
            format_expression(&x.value)
        ),
    };
    return format!(
        "for ({}; {}; {}) {}",
        init,
        format_expression(&node.condition),
        format_expression(&node.action),
        format_body(&node.body)
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

fn format_if(node: &If) -> String {
    let mut s = format!(
        "if ({cond}) {body}",
        cond = format_expression(&node.condition),
        body = format_body(&node.body)
    );
    if node.else_body.is_some() {
        s += &format!(" else {}", format_body(node.else_body.as_ref().unwrap()));
    }
    return s;
}

// fn format_import(node: &Import) -> String {
//     format!("import {}\n", node.path)
// }

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

fn format_module_variable(node: &ModuleVariable) -> String {
    return format!(
        "static {} {} = {};\n",
        format_type(&node.type_name),
        format_form(&node.form),
        format_expression(&node.value)
    );
}

fn format_return(node: &Return) -> String {
    match &node.expression {
        None => String::from("return;"),
        Some(x) => format!("return {}", format_expression(&x)),
    }
}

fn format_struct_fieldlist(node: &StructFieldlist) -> String {
    let mut s = format_type(&node.type_name) + " ";
    for (i, form) in node.forms.iter().enumerate() {
        if i > 0 {
            s += ", ";
        }
        s += &format_form(&form);
    }
    s += ";";
    return s;
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
        Statement::If(x) => format_if(x),
        Statement::For(x) => format_for(x),
        Statement::While(x) => format_while(x),
        Statement::Return(x) => format_return(x) + ";",
        Statement::Switch(x) => format_switch(x),
        Statement::Expression(x) => format_expression(&x) + ";",
    }
}

fn format_switch(node: &Switch) -> String {
    let mut s = String::new();
    for case in &node.cases {
        let val = match &case.value {
            SwitchCaseValue::Identifier(x) => x.clone(),
            SwitchCaseValue::Literal(x) => format_literal(&x),
        };
        s += &format!("case {}: {{\n", val);
        for statement in &case.statements {
            s += &format_statement(&statement);
            s += ";\n";
        }
        s += "}\n";
    }
    if node.default.is_some() {
        s += "default: {\n";
        for statement in node.default.as_ref().unwrap() {
            s += &format_statement(&statement);
            s += ";\n";
        }
        s += "}\n";
    }
    return format!(
        "switch ({}) {{\n{}\n}}",
        format_expression(&node.value),
        indent(&s)
    );
}

pub fn format_typedef(type_name: &TypedefTarget, form: &TypedefForm) -> String {
    let mut formstr = form.stars.clone() + &form.alias;
    if form.params.is_some() {
        formstr += &format_anonymous_parameters(&form.params.as_ref().unwrap());
    }
    if form.size > 0 {
        formstr += &format!("[{}]", form.size);
    }
    let t = match &type_name {
        TypedefTarget::AnonymousStruct(x) => format_anonymous_struct(&x),
        TypedefTarget::Type(x) => format_type(&x),
    };
    return format!("typedef {} {};\n", t, formstr);
}

fn format_while(node: &While) -> String {
    return format!(
        "while ({}) {}",
        format_expression(&node.condition),
        format_body(&node.body)
    );
}

pub fn format_compat_module(node: &CompatModule) -> String {
    let mut s = String::new();
    for e in &node.elements {
        s += &match e {
            CompatModuleObject::ModuleVariable(x) => format_module_variable(&x),
            CompatModuleObject::Typedef {
                type_name, form, ..
            } => format_typedef(&type_name, &form),
            CompatModuleObject::CompatMacro(x) => format_compat_macro(&x),
            CompatModuleObject::CompatInclude(x) => format!("#include {}\n", x),
            CompatModuleObject::Enum { members, .. } => {
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
            CompatModuleObject::CompatStructForwardDeclaration(x) => format!("struct {};\n", x),
            CompatModuleObject::CompatStructDefinition(x) => format_compat_struct_definition(&x),
            CompatModuleObject::CompatFunctionForwardDeclaration(x) => {
                format_compat_function_forward_declaration(&x)
            }
            CompatModuleObject::CompatFunctionDeclaration(x) => {
                format_compat_function_declaration(&x)
            }
            CompatModuleObject::CompatSplit { text } => format!("\n\n{}\n", text),
        }
    }
    return s;
}
