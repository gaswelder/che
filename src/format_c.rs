use crate::c;
use crate::c::*;
use crate::parser;

pub fn format_module(cm: &CModule) -> String {
    let mut s = String::new();
    for e in &cm.elements {
        s += &match e {
            ModElem::DeclVar(x) => format!(
                "static {} {} = {};\n",
                format_type(&x.type_name),
                format_form(&x.form),
                format_expression(&x.value)
            ),
            ModElem::Typedef(x) => format_typedef(&x),
            ModElem::Macro(x) => format!("#{} {}\n", x.name, x.value),
            ModElem::Include(x) => format!("#include {}\n", x),
            ModElem::DefEnum(x) => fmt_enumdef(x),
            ModElem::ForwardStruct(x) => format!("struct {};\n", x),
            ModElem::DefStruct(x) => format_compat_struct_definition(&x),
            ModElem::ForwardFunc(x) => fmt_function_forward_declaration(&x),
            ModElem::FuncDef(x) => fmt_func_def(&x),
        }
    }
    return s;
}

fn fmt_enumdef(x: &c::EnumDef) -> String {
    let mut s1 = String::from("enum {\n");
    for (i, member) in x.entries.iter().enumerate() {
        if i > 0 {
            s1 += ",\n";
        }
        s1 += &format!("\t{}", format_enum_member(&member));
    }
    s1 += "\n};\n";
    s1
}

fn fmt_func_def(x: &c::FuncDef) -> String {
    let form = format_form(&x.form);
    let mut s = String::new();
    if x.is_static && form != "main" {
        s += "static ";
    }
    s += &format!(
        "{} {} {} {}",
        format_type(&x.type_name),
        form,
        format_compat_function_parameters(&x.parameters),
        format_body(&x.body)
    );
    s
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

fn format_expression(expr: &Expr) -> String {
    match expr {
        Expr::Cast { type_name, operand } => {
            return format!(
                "({})({})",
                format_anonymous_typeform(&type_name),
                format_expression(&operand)
            );
        }
        Expr::Call {
            func: function,
            args: arguments,
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
        Expr::Literal(x) => format_literal(x),
        Expr::Ident(x) => x.clone(),
        Expr::CompositeLiteral(CCompositeLiteral { entries }) => {
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
                let v = format_expression(&e.val);
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
        Expr::Sizeof { arg: argument } => {
            let arg = match &**argument {
                SizeofArg::Typename(x) => format_type(&x),
                SizeofArg::Expression(x) => format_expression(&x),
            };
            return format!("sizeof({})", arg);
        }
        Expr::FieldAccess {
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
        Expr::BinaryOp(x) => format_binary_op(&x),
        Expr::PrefixOp { operator, operand } => {
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
                Expr::BinaryOp(x) => {
                    format!("{}({})", operator, format_binary_op(&x))
                }
                Expr::Cast { type_name, operand } => format!(
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
        Expr::PostfixOp { operator, operand } => {
            return format_expression(&operand) + &operator;
        }
        Expr::ArrayIndex { array, index } => {
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

fn is_binary_op(a: &Expr) -> Option<&String> {
    match a {
        Expr::BinaryOp(x) => Some(&x.op),
        _ => None,
    }
}

fn is_op(e: &Expr) -> Option<String> {
    match e {
        Expr::BinaryOp(x) => Some(String::from(&x.op)),
        Expr::PostfixOp { .. } => Some(String::from("prefix")),
        Expr::PrefixOp { .. } => Some(String::from("prefix")),
        _ => None,
    }
}

fn format_binary_op(x: &c::BinaryOp) -> String {
    let op = &x.op;
    let a = &x.a;
    let b = &x.b;
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

fn fmt_function_forward_declaration(x: &c::ForwardFunc) -> String {
    let mut s = String::new();
    let form = format_form(&x.form);
    if x.is_static && form != "main" {
        s += "static ";
    }
    s += &format!(
        "{} {} {};\n",
        format_type(&x.type_name),
        form,
        format_compat_function_parameters(&x.parameters)
    );
    return s;
}

fn format_typeform(x: &CTypeForm) -> String {
    return format!("{} {};\n", format_type(&x.type_name), format_form(&x.form));
}

pub fn format_compat_struct_definition(x: &c::StructDef) -> String {
    let name = &x.name;
    let fields = &x.fields;
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

fn format_enum_member(node: &EnumEntry) -> String {
    let mut s = node.id.clone();
    if node.value.is_some() {
        s += " = ";
        s += &format_expression(node.value.as_ref().unwrap());
    }
    return s;
}

fn format_for(
    init: &Option<ForInit>,
    condition: &Option<Expr>,
    action: &Option<Expr>,
    body: &CBody,
) -> String {
    let init = match init {
        Some(init) => match init {
            ForInit::Expr(x) => format_expression(&x),
            ForInit::DeclLoopCounter(x) => fmt_vardecl(&x),
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

fn fmt_vardecl(x: &c::DeclVar) -> String {
    format!(
        "{} {} = {}",
        format_type(&x.type_name),
        format_form(&x.form),
        format_expression(&x.value)
    )
}

fn format_compat_function_parameters(parameters: &FuncParams) -> String {
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

fn format_statement(node: &Statement) -> String {
    match node {
        Statement::Block { statements } => format_body(&CBody {
            statements: statements.clone(),
        }),
        Statement::Break => format!("break;"),
        Statement::Continue => format!("continue;"),
        Statement::VarDecl {
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
        Statement::While {
            cond: condition,
            body,
        } => {
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
        Statement::Switch(x) => fmt_switch(&x),
        Statement::Expression(x) => format_expression(&x) + ";",
    }
}

fn fmt_switch(x: &c::Switch) -> String {
    let mut s = String::new();
    s.push_str(&format!("switch ({}) {{\n", format_expression(&x.value)));

    for case in &x.cases {
        for (i, v) in case.values.iter().enumerate() {
            if i > 0 {
                s.push_str("\n");
            }
            let valstring = match v {
                CSwitchCaseValue::Ident(x) => x.clone(),
                CSwitchCaseValue::Literal(x) => format_literal(&x),
            };
            s.push_str(&format!("case {}:", valstring));
        }
        s.push_str(" {\n");
        for statement in &case.body.statements {
            s.push_str(&format!("\t{};\n", &format_statement(&statement)));
        }
        s.push_str("\tbreak;\n}\n");
    }
    if let Some(b) = &x.default {
        s.push_str("default: {\n");
        for statement in &b.statements {
            s.push_str(&format!("\t{};\n", &format_statement(statement)));
        }
        s.push_str("}\n");
    }
    s.push_str("}");
    s
}

pub fn format_typedef(x: &c::Typedef) -> String {
    let form = &x.form;
    let t = &x.typename;
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
