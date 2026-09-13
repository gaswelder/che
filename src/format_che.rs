use crate::nodes;
use crate::nodes::*;
use crate::parser;

pub fn fmt_mod(m: &nodes::Module) -> String {
    let mut s = String::new();

    let mut ss = m.imports.clone();
    ss.sort();
    for i in ss {
        s += &format!("#import {}\n", i);
    }
    for e in &m.elements {
        s += "\n";
        s += &format!("{}", fmt_mod_elem(&e));
    }
    s
}

fn fmt_mod_elem(elem: &ModElem) -> String {
    match elem {
        ModElem::Macro(_) => todo!(),
        ModElem::Enum(_) => todo!(),
        ModElem::StructAlias(_) => todo!(),
        ModElem::Typedef(_) => todo!(),
        ModElem::StructTypedef(x) => fmt_struct_typedef(&x),
        ModElem::ModVar(x) => {
            let mut s = String::new();
            s += &fmt_typename(&x.typename);
            s += " ";
            s += &fmt_form(&x.form);
            s += " = ";
            let v = x.value.clone().unwrap();
            s += &fmt_expr(&v);
            s += ";\n";
            s
        }
        ModElem::FuncDecl(x) => fmt_func(&x),
    }
}

fn fmt_struct_typedef(x: &StructTypedef) -> String {
    let mut s = String::new();
    if x.ispub {
        s += "pub ";
    }
    s += "typedef {\n";
    for e in &x.entries {
        match e {
            StructEntry::Plain(p) => {
                s += "\t";
                s += &fmt_typename(&p.typename);
                s += " ";
                for (i, n) in p.forms.iter().enumerate() {
                    if i > 0 {
                        s += ", ";
                    }
                    s += &fmt_form(&n);
                }
                if let Some(c) = &p.trailing_comment {
                    s += &format!("; // {}\n", c);
                } else {
                    s += ";\n";
                }
            }
            StructEntry::Union(_) => todo!(),
        }
    }
    s += "} ";
    s += &x.name;
    s += ";\n";
    s
}

fn fmt_func(x: &FuncDecl) -> String {
    let mut s = String::new();
    s += &fmt_comments(&x.comments);
    if x.ispub {
        s += "pub ";
    }
    s += &fmt_typename(&x.typename);
    s += " ";
    s += &fmt_form(&x.form);
    s += "(";
    for (i, p) in x.params.list.iter().enumerate() {
        if i > 0 {
            s += ", ";
        }
        s += &fmt_typename(&p.typename);
        s += " ";
        for (i, n) in p.forms.iter().enumerate() {
            if i > 0 {
                s += ", ";
            }
            s += &fmt_form(&n);
        }
    }
    s += ") {\n";
    for st in &x.body.statements {
        s += &format!("{}\n", indent(&fmt_statement(&st)));
    }
    s += "}\n";
    s
}

fn fmt_statement(s: &FunctionElement) -> String {
    match s {
        FunctionElement::Break => String::from("break;"),
        FunctionElement::Continue => String::from("continue;"),
        FunctionElement::VarDecl(x) => fmt_vardecl(x),
        FunctionElement::If(x) => fmt_if(&x),
        FunctionElement::For(x) => {
            let mut s = String::new();
            s += "for (";
            if let Some(init) = &x.init {
                match init {
                    ForInit::Expr(expr) => {
                        s += &fmt_expr(&expr);
                    }
                    ForInit::DeclLoopCounter {
                        type_name,
                        form,
                        value,
                    } => {
                        s += &fmt_typename(&type_name);
                        s += " ";
                        s += &fmt_form(&form);
                        s += " = ";
                        s += &fmt_expr(&value);
                    }
                }
            }
            s += ";";
            if let Some(e) = &x.condition {
                s += " ";
                s += &fmt_expr(&e);
            }
            s += ";";
            if let Some(e) = &x.action {
                s += " ";
                s += &fmt_expr(&e);
            }
            s += ") {\n";
            for st in &x.body.statements {
                s += &format!("{}\n", indent(&fmt_statement(&st)));
            }
            s += "}";
            s
        }
        FunctionElement::While(x) => {
            let mut s = String::new();
            s += &format!("while ({}) {{\n", fmt_expr(&x.cond));
            for st in &x.body.statements {
                s += &format!("{}\n", indent(&fmt_statement(&st)))
            }
            s += "}";
            s
        }
        FunctionElement::Return(x) => match &x.expression {
            Some(e) => format!("return {};", fmt_expr(&e)),
            None => format!("return;"),
        },
        FunctionElement::Switch(x) => fmt_switch(&x),
        FunctionElement::Statement(x) => {
            if let Some(c) = &x.trailing_comment {
                format!("{}; // {}", fmt_expr(&x.expr), c)
            } else {
                format!("{};", fmt_expr(&x.expr))
            }
        }
    }
}

fn fmt_comments(x: &Option<Vec<String>>) -> String {
    let mut s = String::new();
    if x.is_none() {
        return s;
    }
    for line in x.as_ref().unwrap() {
        s += &format!("// {}\n", line);
    }
    s
}

fn fmt_vardecl(x: &VarDecl) -> String {
    let mut s = String::new();
    s += &fmt_comments(&x.comments);
    s += &fmt_typename(&x.typename);
    s += " ";
    s += &fmt_form(&x.form);
    if let Some(e) = &x.value {
        s += " = ";
        s += &fmt_expr(&e);
    }
    s += ";";
    if let Some(c) = &x.trailing_comment {
        s += &format!(" // {}", c);
    }
    s
}

fn fmt_switch(x: &Switch) -> String {
    let mut s = String::new();
    s += &format!("switch ({}) {{\n", fmt_expr(&x.value));
    s += &indent(&fmt_cases(x));
    s += "\n}";
    s
}

fn fmt_cases(x: &Switch) -> String {
    let mut s = String::new();
    for (casei, c) in x.cases.iter().enumerate() {
        if casei > 0 {
            s += "\n";
        }
        s += "case ";
        for (i, v) in c.values.iter().enumerate() {
            if i > 0 {
                s += ", ";
            }
            match v {
                SwitchCaseValue::Ident(x) => s += &fmt_nsname(&x),
                SwitchCaseValue::Literal(literal) => s += &fmt_literal(&literal),
            }
        }
        let n = c.body.statements.len();
        match n {
            0 => {
                s += ": {}";
            }
            1 => {
                s += ": { ";
                for st in &c.body.statements {
                    s += &fmt_statement(&st);
                }
                s += " }";
            }
            _ => {
                s += ": {\n";
                for st in &c.body.statements {
                    s += &indent(&fmt_statement(&st));
                    s += "\n";
                }
                s += "}";
            }
        }
        if let Some(c) = &c.body.trailing_comment {
            s += &format!(" // {}", c);
        }
    }
    if let Some(c) = &x.default_case {
        s += "\ndefault";
        let n = c.statements.len();
        match n {
            0 => {
                s += ": {\n";
                s += "}";
            }
            1 => {
                s += ": { ";
                for st in &c.statements {
                    s += &format!("{}", fmt_statement(&st));
                }
                s += " }";
            }
            _ => {
                s += ": {\n";
                for st in &c.statements {
                    s += &format!("\t{}\n", fmt_statement(&st));
                }
                s += "}";
            }
        }
    }
    s
}

fn fmt_if(x: &If) -> String {
    let mut s = String::new();
    s += &fmt_comments(&x.comments);
    s += &format!("if ({}) {{\n", fmt_expr(&x.condition));
    for st in &x.body.statements {
        s += &format!("{}\n", &indent(&fmt_statement(&st)));
    }
    if let Some(e) = &x.else_body {
        dbg!(e);
        s += "} else {\n";
        s += "\tbleble;\n";
        todo!();
    }
    s += "}";
    s
}

pub fn fmt_field_access(x: &FieldAccess) -> String {
    format!("{}{}{}", fmt_expr(&x.target), &x.op, &x.field_name)
}

fn fmt_nsname(x: &NsName) -> String {
    if x.ns == "" {
        format!("{}", &x.name)
    } else {
        format!("{}.{}", &x.ns, &x.name)
    }
}

pub fn fmt_expr(expr: &Expr) -> String {
    match expr {
        Expr::FieldAccess(x) => fmt_field_access(x),
        Expr::Cast(x) => fmt_cast(&x),
        Expr::NsName(x) => fmt_nsname(&x),
        Expr::Call(x) => fmt_call(&x),
        Expr::Literal(x) => fmt_literal(x),
        Expr::CompositeLiteral(x) => fmt_composite_literal(x),
        Expr::Sizeof(x) => {
            let argument = &x.arg;
            let arg = match &**argument {
                SizeofArg::Typename(x) => fmt_bare_typeform(&x),
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
                            fmt_bare_typeform(&type_name),
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

fn fmt_call(x: &Call) -> String {
    let mut s = String::new();
    if let Some(c) = &x.comments {
        for line in c {
            s += &format!("// {}\n", line);
        }
    }
    s += &fmt_expr(&x.func);
    s += "(";
    for (i, arg) in x.args.iter().enumerate() {
        if i > 0 {
            s += ", ";
        }
        s += &fmt_expr(&arg);
    }
    s += ")";
    s
}

fn fmt_composite_literal(x: &CompLiteral) -> String {
    let entries = &x.entries;
    if entries.len() == 0 {
        return String::from("{}");
    }

    let mut totalwidth = 0;
    let mut maxwidth = 0;
    let mut items = Vec::new();
    for e in &x.entries {
        let v = fmt_expr(&e.value);
        let item = match &e.key {
            Some(expr) => {
                let k = fmt_expr(expr);
                if e.is_index {
                    format!("[{}] = {}", k, v)
                } else {
                    format!(".{} = {}", k, v)
                }
            }
            None => v,
        };
        let n = item.len();
        totalwidth += n;
        if n > maxwidth {
            maxwidth = n;
        }
        items.push(item);
    }

    let mut s = String::new();
    if totalwidth < 100 {
        s += "{ ";
        for (i, item) in items.iter().enumerate() {
            if i > 0 {
                s += ", ";
            }
            s += item
        }
        s += " }";
    } else if maxwidth < 5 {
        s += "{\n";
        for (i, item) in items.iter().enumerate() {
            let padded = format!("{:>maxwidth$}", item);
            if i == 0 {
                s += "\t";
                s += &padded;
                continue;
            }
            if i % 8 == 0 {
                s += ",\n\t";
            } else {
                s += ", ";
            }
            s += &padded
        }
        s += "\n}";
    } else {
        s += "{\n";
        for (i, item) in items.iter().enumerate() {
            if i > 0 {
                s += ",\n";
            }
            s += "\t";
            s += item
        }
        s += "\n}";
    }

    s
}

fn fmt_cast(x: &Cast) -> String {
    let t = fmt_bare_typeform(&x.typeform);
    let arg = fmt_expr(&x.operand);
    let nobr = format!("({}) {}", t, arg);
    let br = format!("({})({})", t, arg);
    match *x.operand {
        Expr::Literal(_) => nobr,
        Expr::NsName(_) => nobr,
        _ => br,
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

fn fmt_bare_typeform(x: &BareTypeform) -> String {
    format!("{}{}", fmt_typename(&x.typename), "*".repeat(x.hops))
}

pub fn fmt_binop(x: &BinaryOp) -> String {
    let op = &x.op;
    let isop1 = is_op(&x.a);
    let isop2 = is_op(&x.b);
    let s1 = fmt_expr(&x.a);
    let s2 = fmt_expr(&x.b);

    // let no = format!("{} {} {}", &s1, &op, &s2);

    let wrap1 = format!("({})", &s1);
    let wrap2 = format!("({})", &s2);

    let left = match (isop1.as_deref(), op.as_str()) {
        (None, _) => &s1,
        (Some("-"), "-") => &s1,
        (Some("+"), "-") => &s1,

        (Some("*"), "+") => &s1,
        (Some("*"), "-") => &s1,
        (Some("*"), "*") => &s1,
        (Some("*"), "/") => &s1,

        (_, "&&") => &s1,
        (_, "||") => &s1,

        _ => &wrap1,
    };
    let right = match (op.as_str(), isop2.as_deref()) {
        (_, None) => &s2,
        ("&&", _) => &s2,
        ("||", _) => &s2,

        ("-", Some("*")) => &s2,
        ("+", Some("*")) => &s2,

        ("=", _) => &s2,
        (_, Some("prefix")) => &s2,

        _ => &wrap2,
    };
    return format!("{} {} {}", &left, &op, &right);
}

pub fn fmt_binop0(x: &nodes::BinaryOp) -> String {
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
        Literal::String(val) => {
            let parts: Vec<String> = val.iter().map(|x| format!("\"{}\"", x)).collect();
            return parts.join("");
        }
        Literal::Number(val) => format!("{}", val),
        Literal::Null => String::from("NULL"),
    }
}

fn indent(s: &str) -> String {
    let lines: Vec<String> = s.split("\n").map(|line| format!("\t{}", line)).collect();
    lines.join("\n")
}
