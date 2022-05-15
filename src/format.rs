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

fn format_type(node: &Type) -> String {
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
        Expression::BinaryOp { op, a, b } => format_binary_op(op, a, b),
        Expression::Cast(x) => format_cast(x),
        Expression::FunctionCall(x) => format_function_call(x),
        Expression::Expression(x) => format_expression(x),
        Expression::Literal(x) => format_literal(x),
        Expression::Identifier(x) => x.clone(),
        Expression::StructLiteral(x) => format_struct_literal(x),
        Expression::ArrayLiteral(x) => format_array_literal(x),
        Expression::Sizeof(x) => format_sizeof(x),
        Expression::PrefixOperator(x) => format_prefix_operator(x),
        Expression::PostfixOperator(x) => format_postfix_operator(x),
        Expression::ArrayIndex(x) => format_array_index(x),
    }
}

fn format_cast(node: &Cast) -> String {
    return format!(
        "({}) {}",
        format_anonymous_typeform(&node.type_name),
        format_expression(&node.operand)
    );
}

fn format_sizeof(node: &Sizeof) -> String {
    let arg = match &node.argument {
        SizeofArgument::Type(x) => format_type(&x),
        SizeofArgument::Expression(x) => format_expression(&x),
    };
    return format!("sizeof({})", arg);
}

fn format_anonymous_typeform(node: &AnonymousTypeform) -> String {
    let mut s = format_type(&node.type_name);
    for op in &node.ops {
        s += &op;
    }
    return s;
}

fn format_array_index(node: &ArrayIndex) -> String {
    return format!(
        "{}[{}]",
        format_expression(&node.array),
        format_expression(&node.index)
    );
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
        Expression::Expression(e) => is_binary_op(e),
        _ => None,
    }
}

fn format_binary_op(op: &String, a: &Expression, b: &Expression) -> String {
    // If a is a binary op weaker than op, wrap it
    let af = match is_binary_op(a) {
        Some(k) => {
            if parser::operator_strength(&k) < parser::operator_strength(op) {
                format!("({})", format_expression(a))
            } else {
                format_expression(a)
            }
        }
        None => format_expression(a),
    };
    // If b is a binary op weaker than op, wrap it
    let bf = match is_binary_op(b) {
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

fn format_compat_include(node: &CompatInclude) -> String {
    return format!("#include {}\n", node.name);
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

fn format_compat_struct_forward_declaration(node: &CompatStructForwardDeclaration) -> String {
    return format!("struct {};\n", node.name);
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

// fn format_enum(node: &Enum) -> String {
//     let mut s = String::new();
//     if node.is_pub {
//         s += "pub ";
//     }
//     s += "enum {\n";
//     for id in &node.members {
//         s += &format!("\t{},\n", format_enum_member(&id));
//     }
//     s += "}\n";
//     return s;
// }

fn format_enum_member(node: &EnumMember) -> String {
    let mut s = node.id.clone();
    if node.value.is_some() {
        s += " = ";
        s += &format_literal(node.value.as_ref().unwrap());
    }
    return s;
}

fn format_for(node: &For) -> String {
    let init = match &node.init {
        ForInit::Expression(x) => format_expression(&x),
        ForInit::LoopCounterDeclaration(x) => format!(
            "{} {} = {}",
            format_type(&x.type_name),
            x.name,
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

fn format_function_call(node: &FunctionCall) -> String {
    let mut s1 = String::from("(");
    for (i, argument) in node.arguments.iter().enumerate() {
        if i > 0 {
            s1 += ", ";
        }
        s1 += &format_expression(&argument);
    }
    s1 += ")";
    return format!("{}{}", format_expression(&node.function), s1);
}

// fn format_function_declaration(node: &FunctionDeclaration) -> String {
//     let s = format!(
//         "{} {}{} {}\n\n",
//         format_type(&node.type_name),
//         format_form(&node.form),
//         format_function_parameters(&node.parameters),
//         format_body(&node.body)
//     );
//     if node.is_pub {
//         return format!("pub {}", s);
//     }
//     return s;
// }

// fn format_function_parameters(parameters: &FunctionParameters) -> String {
//     let mut s = String::from("(");
//     for (i, parameter) in parameters.list.iter().enumerate() {
//         if i > 0 {
//             s += ", ";
//         }
//         s += &format_type(&parameter.type_name);
//         s += " ";
//         for (i, form) in parameter.forms.iter().enumerate() {
//             if i > 0 {
//                 s += ", ";
//             }
//             s += &format_form(&form);
//         }
//     }
//     if parameters.variadic {
//         s += ", ...";
//     }
//     s += ")";
//     return s;
// }

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

// fn format_module(node: &Module) -> String {
//     let mut s = String::new();
//     for cnode in &node.elements {
//         s += &match cnode {
//             ModuleObject::CompatInclude(x) => format_compat_include(&x),
//             ModuleObject::ModuleVariable(x) => format_module_variable(&x),
//             ModuleObject::Enum(x) => format_enum(&x),
//             ModuleObject::FunctionDeclaration(x) => format_function_declaration(&x),
//             ModuleObject::Import(x) => format_import(&x),
//             ModuleObject::Typedef(x) => format_typedef(&x),
//             ModuleObject::CompatMacro(x) => format_compat_macro(&x),
//         }
//     }
//     return s;
// }

fn format_module_variable(node: &ModuleVariable) -> String {
    return format!(
        "{} {} = {};\n",
        format_type(&node.type_name),
        format_form(&node.form),
        format_expression(&node.value)
    );
}

fn format_postfix_operator(node: &PostfixOperator) -> String {
    return format_expression(&node.operand) + &node.operator;
}

fn format_prefix_operator(node: &PrefixOperator) -> String {
    let operand = &node.operand;
    let operator = &node.operator;
    match operand {
        Expression::BinaryOp { op, a, b } => {
            format!("{}({})", operator, format_binary_op(op, a, b))
        }
        Expression::Cast(x) => format!("{}({})", operator, format_cast(&*x)),
        _ => format!("{}{}", operator, format_expression(&operand)),
    }
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

fn format_struct_literal(node: &StructLiteral) -> String {
    let mut s = String::from("{\n");
    for member in &node.members {
        s += &format!(
            "\t.{} = {},\n",
            member.name,
            format_expression(&member.value)
        );
    }
    s += "}\n";
    return s;
}

fn format_statement(node: &Statement) -> String {
    match node {
        Statement::VariableDeclaration(x) => format_variable_declaration(x) + ";",
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

pub fn format_typedef(node: &Typedef) -> String {
    let mut form = node.form.stars.clone() + &node.form.alias;
    if node.form.params.is_some() {
        form += &format_anonymous_parameters(&node.form.params.as_ref().unwrap());
    }
    if node.form.size > 0 {
        form += &format!("[{}]", node.form.size);
    }
    let t = match &node.type_name {
        TypedefTarget::AnonymousStruct(x) => format_anonymous_struct(&x),
        TypedefTarget::Type(x) => format_type(&x),
    };
    return format!("typedef {} {};\n", t, form);
}

fn format_variable_declaration(node: &VariableDeclaration) -> String {
    let mut s = String::from(format_type(&node.type_name)) + " ";
    for (i, form) in node.forms.iter().enumerate() {
        let value = &node.values[i];
        if i > 0 {
            s += ", ";
        }
        s += &format!("{} = {}", &format_form(&form), &format_expression(value));
    }
    return s;
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
            CompatModuleObject::Typedef(x) => format_typedef(&x),
            CompatModuleObject::CompatMacro(x) => format_compat_macro(&x),
            CompatModuleObject::CompatInclude(x) => format_compat_include(&x),
            CompatModuleObject::Enum { members, .. } => {
                let mut s = String::from("enum {\n");
                for (i, member) in members.iter().enumerate() {
                    if i > 0 {
                        s += ",\n";
                    }
                    s += &format!("\t{}", format_enum_member(&member));
                }
                s += "\n};\n";
                return s;
            }
            CompatModuleObject::CompatStructForwardDeclaration(x) => {
                format_compat_struct_forward_declaration(&x)
            }
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