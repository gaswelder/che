use substring::Substring;

use crate::nodes::*;
use crate::parser;

pub fn fmt_mod(m: &Module) -> String {
    // Separate imports from other elements.
    let mut imports = Vec::new();
    let mut rest = Vec::new();
    for e in &m.elements {
        match e {
            ModElem::Import(import) => imports.push(import),
            _ => rest.push(e),
        }
    }

    // Treat the current first import's comment as the module comment.
    let mut module_comment = None;
    if !imports.is_empty() {
        module_comment = imports[0].source_info.comments.clone();
    }

    let mut s = fmt_comments(&module_comment);

    // Reorder the imports.
    imports.sort_by_key(|m| &m.path);
    for e in &imports {
        s += "#import ";
        s += &e.path;
        s += "\n";
    }

    // Format the rest.
    for e in &rest {
        s += &format!("{}", fmt_mod_elem(&e));
    }
    s
}

fn fmt_mod_elem(elem: &ModElem) -> String {
    match elem {
        ModElem::Import(_) => {
            panic!("shouldn't happen")
        }
        ModElem::Macro(x) => format!("#{}{}\n", x.name, x.value),
        ModElem::Enum(x) => fmt_enum(&x),
        ModElem::StructAlias(_) => todo!(),
        ModElem::Typedef(x) => fmt_typedef(&x),
        ModElem::StructTypedef(x) => fmt_struct_typedef(&x),
        ModElem::ModVar(x) => format!("{}\n", &fmt_var(&x)),
        ModElem::FuncDecl(x) => fmt_func(&x),
    }
}

fn fmt_enum(x: &EnumDecl) -> String {
    let mut s = fmt_begin(&x.source_info);
    s += "enum {\n";

    let mut table = Vec::new();

    for e in &x.entries {
        let mut line = String::new();
        line += &e.name;
        if let Some(v) = &e.val {
            line += " = ";
            line += &fmt_expr(&v);
        }
        line += &fmt_end(&e.source_info);

        let cols: Vec<String> = line
            .splitn(2, "//")
            .map(|x| String::from(x.trim()))
            .collect();
        if cols.len() == 1 {
            table.push((cols[0].clone(), String::new()));
        } else {
            table.push((cols[0].clone(), cols[1].clone()));
        }
    }

    let width = table.iter().map(|x| x.0.len()).max().unwrap();

    for i in 0..x.entries.len() {
        let (a, b) = &table[i];
        s += &indent(&fmt_begin(&x.entries[i].source_info));
        s += &format!("\t{},", a);
        if b != "" {
            s += &" ".repeat(width - a.len());
            s += " // ";
            s += b;
        }
        s += "\n";
    }

    s += "}\n";
    s += &fmt_end(&x.source_info);
    s
}

fn fmt_typedef(x: &Typedef) -> String {
    let mut s = String::new();
    if x.ispub {
        s += "pub ";
    }
    s += "typedef ";
    s += &fmt_typename(&x.typename);
    s += " ";
    s += &x.alias;
    if let Some(p) = &x.func_params {
        s += "(";
        for (i, a) in p.forms.iter().enumerate() {
            if i > 0 {
                s += ", ";
            }
            s += &fmt_bare_typeform(&a);
        }
        s += ")";
    }
    s += ";\n";
    s
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
    let mut s = fmt_begin(&x.source_info);
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
    for st in &x.body.items {
        s += &indent(&fmt_block_item(&st));
        s += "\n";
    }
    s += "}\n";
    s
}

fn fmt_block_item(s: &BlockItem) -> String {
    match s {
        BlockItem::Break => String::from("break;"),
        BlockItem::Continue => String::from("continue;"),
        BlockItem::For(x) => fmt_for(&x),
        BlockItem::If(x) => fmt_if(&x),
        BlockItem::Return(x) => fmt_return(&x),
        BlockItem::Statement(x) => fmt_statement(&x),
        BlockItem::Switch(x) => fmt_switch(&x),
        BlockItem::VarDecl(x) => fmt_var(x),
        BlockItem::While(x) => fmt_while(&x),
    }
}

fn fmt_var(x: &VarDecl) -> String {
    let mut s = String::new(); // fmt_begin(&x.source_info);
    s += &fmt_typename(&x.typename);
    s += " ";
    s += &fmt_form(&x.form);
    if let Some(e) = &x.value {
        s += " = ";
        s += &fmt_expr(&e);
    }
    s += ";";
    s += &fmt_end(&x.source_info);
    s
}

fn fmt_while(x: &While) -> String {
    let mut s = fmt_begin(&x.source_info);
    s += "while (";
    s += &fmt_expr(&x.cond);
    s += ") ";
    s += &fmt_block(&x.body);
    s
}

fn fmt_for(x: &For) -> String {
    let mut s = fmt_begin(&x.source_info);
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
    s += ") ";
    s += &fmt_block(&x.body);
    s
}

fn fmt_statement(x: &Statement) -> String {
    let mut s = String::new();
    //fmt_begin(&x.source_info);
    s += &fmt_expr(&x.expr);
    s += ";";
    s += &fmt_end(&x.source_info);
    s
}

fn fmt_return(x: &Return) -> String {
    let mut s = fmt_begin(&x.source_info);
    s += "return";
    if let Some(e) = &x.expression {
        s += " ";
        s += &fmt_expr(&e);
    }
    s += ";";
    s
}

fn fmt_switch(x: &Switch) -> String {
    let mut s = fmt_begin(&x.source_info);
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
        s += ": ";
        let n = c.body.items.len();
        match n {
            0 => {
                s += "{}";
            }
            1 => {
                s += "{ ";
                s += &fmt_block_item(&c.body.items[0]);
                s += " }";
            }
            _ => {
                s += &fmt_block(&c.body);
            }
        }
        if let Some(c) = &c.body.trailing_comment {
            s += &format!(" // {}", c);
        }
    }
    if let Some(c) = &x.default_case {
        s += "\ndefault: ";
        let n = c.items.len();
        match n {
            0 => {
                s += "{}\n";
            }
            1 => {
                s += "{ ";
                s += &fmt_block_item(&c.items[0]);
                s += " }";
            }
            _ => {
                s += &fmt_block(&c);
            }
        }
    }
    s
}

fn fmt_if(x: &If) -> String {
    let mut s = fmt_begin(&x.source_info);
    s += &format!("if ({}) ", fmt_expr(&x.condition));
    s += &fmt_block(&x.body);
    if x.else_body.is_none() {
        return s;
    }
    let e = x.else_body.clone().unwrap();

    s += " else ";

    let mut single_nested_if = None;
    if e.items.len() == 1 {
        if let BlockItem::If(x) = &e.items[0] {
            single_nested_if = Some(x);
        }
    }
    if single_nested_if.is_some() {
        s += &fmt_if(single_nested_if.unwrap());
        return s;
    }

    s += &fmt_block(&e);
    s
}

fn fmt_block(x: &Body) -> String {
    let mut s = String::new();
    s += "{\n";
    for st in &x.items {
        s += &indent(&fmt_block_item(&st));
        s += "\n";
    }
    s += "}";
    s
}

pub fn fmt_field_access(x: &FieldAccess) -> String {
    format!("{}{}{}", fmt_expr(&x.target), &x.op, &x.field_name)
}

fn fmt_nsname(x: &NsName) -> String {
    let mut s = String::new();
    if x.source_info.is_some() {
        s += &fmt_begin(x.source_info.as_ref().unwrap());
    }
    if !x.ns.is_empty() {
        s += &format!("{}.", &x.ns);
    }
    s += &x.name;
    if x.source_info.is_some() {
        s += &fmt_end(x.source_info.as_ref().unwrap());
    }
    s
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
            format!("sizeof({})", arg)
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
            match expr {
                Expr::BinaryOp(x) => {
                    format!("{}({})", operator, fmt_binop(&x))
                }
                Expr::Cast(x) => {
                    let type_name = &x.typeform;
                    let operand = &x.operand;
                    format!(
                        "{}({})({})",
                        operator,
                        fmt_bare_typeform(&type_name),
                        fmt_expr(&operand)
                    )
                }
                _ => format!("{}{}", operator, fmt_expr(&operand)),
            }
        }
        Expr::PostfixOperator(x) => {
            let operand = &x.operand;
            let operator = &x.operator;
            fmt_expr(&operand) + &operator
        }
        Expr::ArrIndex(x) => {
            format!("{}[{}]", fmt_expr(&x.array), fmt_expr(&x.index))
        }
    }
}

fn fmt_call(x: &Call) -> String {
    let mut s = String::new();
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
    // s += &format!("// tw=${totalwidth}, mw=${maxwidth}\n");
    if totalwidth < 60 {
        s += "{ ";
        for (i, item) in items.iter().enumerate() {
            if i > 0 {
                s += ", ";
            }
            s += item
        }
        s += " }";
    } else if maxwidth < 6 {
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
        Expr::Call(_) => nobr,
        Expr::ArrIndex(_) => nobr,
        Expr::PrefixOperator(_) => nobr,
        _ => br,
    }
}

pub fn fmt_typename(x: &Typename) -> String {
    let mut s = fmt_begin(&x.source_info);
    if x.is_const {
        s += "const ";
    }
    if !x.name.ns.is_empty() {
        s += &format!("{}.", x.name.ns);
    };
    s += &x.name.name;
    s
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
    let mut s = String::new();
    s += &fmt_typename(&x.typename);
    if x.hops > 0 {
        s += " ";
        s += &"*".repeat(x.hops);
    }
    s
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

        (Some("prefix"), "=") => &s1,

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
        ("+=", _) => &s2,
        ("-=", _) => &s2,
        ("*=", _) => &s2,
        ("/=", _) => &s2,

        (_, Some("prefix")) => &s2,

        _ => &wrap2,
    };
    return format!("{} {} {}", &left, &op, &right);
}

fn fmt_literal(node: &Literal) -> String {
    match node {
        Literal::Char(val) => format!("\'{}\'", val),
        Literal::String(val) => {
            let parts: Vec<String> = val.iter().map(|x| format!("\"{}\"", x)).collect();
            return parts.join("");
        }
        Literal::Number(x) => {
            let mut s = String::new();
            s += &x.val;
            s += &fmt_end(&x.source_info);
            s
        }
        Literal::Null => String::from("NULL"),
    }
}

fn indent(s: &str) -> String {
    let lines: Vec<String> = s
        .split("\n")
        .map(|line| {
            if line.trim().len() == 0 {
                return String::new();
            }
            return format!("\t{}", line);
        })
        .collect();
    lines.join("\n")
}

fn fmt_begin(x: &SourceInfo) -> String {
    let mut s = String::new();
    // s += "spaces=[";
    // s += &x
    //     .spaces_top
    //     .replace("\n", "\\n")
    //     .replace(" ", "\\s")
    //     .replace("\t", "\\t");
    // s += "]";
    let n = x.spaces_top.matches('\n').count();
    if n > 1 {
        s += "\n";
    }
    s += &fmt_comments(&x.comments);
    s
}

fn fmt_end(x: &SourceInfo) -> String {
    let mut s = String::new();
    if let Some(c) = &x.trailing_comment {
        if c.starts_with("/*") && c.find('\n').is_none() {
            let cleaned = c.substring(2, c.len() - 2).trim();
            s += &format!(" // {}", cleaned);
        } else {
            s += &format!(" {}", c);
        }
    }
    s
}

fn fmt_comments(x: &Option<Vec<String>>) -> String {
    let mut s = String::new();
    if x.is_none() {
        return s;
    }
    for comment in x.as_ref().unwrap() {
        s += comment;
        s += "\n";
    }
    s
}
