use crate::build::Ctx;
use crate::lexer::{Lexer, Token};
use crate::nodes::*;

pub fn parse_module(l: &mut Lexer, ctx: &Ctx) -> Result<Module, String> {
    return parse_module0(l, &ctx).map_err(|err| {
        let next = l.peek().unwrap();
        let position = format!("{}: {}", &ctx.path, l.peek().unwrap().pos);
        format!("{}: {}: {}...", position, err, token_to_string(next))
    });
}

fn parse_module0(l: &mut Lexer, ctx: &Ctx) -> Result<Module, String> {
    let mut module_objects: Vec<ModuleObject> = vec![];
    while l.more() {
        match l.peek().unwrap().kind.as_str() {
            "import" => {
                l.get();
                // All is in the context already.
            }
            "macro" => {
                module_objects.push(parse_compat_macro(l)?);
            }
            _ => {
                module_objects.push(parse_module_object(l, ctx)?);
            }
        }
    }
    return Ok(Module {
        elements: module_objects,
    });
}

fn parse_compat_macro(lexer: &mut Lexer) -> Result<ModuleObject, String> {
    let content = expect(lexer, "macro", None)
        .unwrap()
        .content
        .unwrap()
        .clone();
    let pos = content.find(" ");
    if pos.is_none() {
        return Err(format!("can't get macro name from '{}'", content));
    }
    let (name, value) = content.split_at(pos.unwrap());

    return Ok(ModuleObject::Macro {
        name: name[1..].to_string(),
        value: value.to_string(),
    });
}

fn parse_module_object(l: &mut Lexer, ctx: &Ctx) -> Result<ModuleObject, String> {
    let mut is_pub = false;
    if l.follows("pub") {
        l.get();
        is_pub = true;
    }
    if l.follows("enum") {
        return parse_enum(l, is_pub, ctx);
    }
    if l.follows("typedef") {
        return parse_typedef(is_pub, l, ctx);
    }
    let type_name = parse_typename(l, None, ctx)?;
    let form = parse_form(l, ctx)?;
    if l.peek().unwrap().kind == "(" {
        return parse_function_declaration(l, is_pub, type_name, form, ctx);
    }

    if l.peek().unwrap().kind != "=" {
        return Err("module variable: '=' expected".to_string());
    }

    if l.peek().unwrap().kind == "=" {
        if is_pub {
            return Err("module variables can't be exported".to_string());
        }
        l.get();
        let value = parse_expr(l, 0, ctx)?;
        expect(l, ";", Some("module variable declaration"))?;
        return Ok(ModuleObject::ModuleVariable {
            type_name,
            form,
            value,
        });
    }
    return Err("unexpected input".to_string());
}

fn parse_expr(l: &mut Lexer, strength: usize, ctx: &Ctx) -> Result<Expression, String> {
    let prefix_strength = operator_strength("prefix");
    if strength < prefix_strength {
        return parse_seq_expr(l, strength, ctx);
    }
    if strength == prefix_strength {
        return parse_prefix_expr(l, ctx);
    }
    if strength == operator_strength("suffix") {
        return base(l, ctx);
    }
    panic!("unexpected n: {}", strength)
}

fn parse_seq_expr(l: &mut Lexer, strength: usize, ctx: &Ctx) -> Result<Expression, String> {
    let mut r = parse_expr(l, strength + 1, ctx)?;
    while l.more() {
        // Continue if next is an operator with the same strength.
        let next = &l.peek().unwrap().kind;
        if !is_op(&next) || operator_strength(&next) != strength {
            break;
        }
        let op = l.get().unwrap().kind;
        let r2 = parse_expr(l, strength + 1, ctx)?;
        r = Expression::BinaryOp {
            op,
            a: Box::new(r),
            b: Box::new(r2),
        }
    }
    return Ok(r);
}

fn parse_prefix_expr(l: &mut Lexer, ctx: &Ctx) -> Result<Expression, String> {
    let token = l.get().unwrap();

    // *{we are here}*foo
    if is_prefix_op(&token.kind) {
        let r = parse_prefix_expr(l, ctx)?;
        return Ok(Expression::PrefixOperator {
            operator: token.kind,
            operand: Box::new(r),
        });
    }

    let next = String::from(&token.kind);
    // cast
    // * (foo_t)x
    if next == "("
        && l.peek().unwrap().kind == "word"
        && is_type(l.peek().unwrap().content.as_ref().unwrap(), &ctx.typenames)
    {
        let typeform = parse_anonymous_typeform(l, ctx)?;
        expect(l, ")", Some("typecast"))?;
        return Ok(Expression::Cast {
            type_name: typeform,
            operand: Box::new(parse_prefix_expr(l, ctx)?),
        });
    }

    if token.kind == "sizeof" {
        l.unget(token);
        return parse_sizeof(l, ctx);
    }

    l.unget(token);
    return base(l, ctx);
}

fn base(l: &mut Lexer, ctx: &Ctx) -> Result<Expression, String> {
    // println!("      base(): {:?}", l.peek());
    let mut r = expr_id(l, ctx)?;
    while l.more() {
        let next = l.peek().unwrap();
        // call
        if next.kind == "(" {
            r = parse_function_call(l, ctx, r)?;
            continue;
        }

        // index
        if next.kind == "[" {
            expect(l, "[", Some("array index"))?;
            let index = parse_expr(l, 0, ctx)?;
            expect(l, "]", Some("array index"))?;
            r = Expression::ArrayIndex {
                array: Box::new(r),
                index: Box::new(index),
            };
            continue;
        }

        if is_postfix_op(&next.kind) {
            r = Expression::PostfixOperator {
                operator: l.get().unwrap().kind,
                operand: Box::new(r),
            };
            continue;
        }

        if next.kind == "->" || next.kind == "." {
            r = Expression::BinaryOp {
                op: l.get().unwrap().kind,
                a: Box::new(r),
                b: Box::new(expr_id(l, ctx)?),
            };
            continue;
        }

        break;
    }
    return Ok(r);
}

fn type_follows(l: &Lexer, ctx: &Ctx) -> bool {
    if !l.peek().is_some() || l.peek().unwrap().kind != "word" {
        return false;
    }
    // <mod>.<name> where <mod> is any imported module that has exported <name>?
    if l.peek_n(2).is_some()
        && l.peek_n(1).unwrap().kind == "."
        && l.peek_n(2).unwrap().kind == "word"
    {
        let a = l.peek_n(0).unwrap();
        let c = l.peek_n(2).unwrap();

        let pos = ctx
            .deps
            .iter()
            .position(|x| x.ns == *a.content.as_ref().unwrap());
        dbg!(&a, &c, &pos, ctx);
        if pos.is_some()
            && ctx.deps[pos.unwrap()]
                .typenames
                .contains(c.content.as_ref().unwrap())
        {
            return true;
        }
    }
    // any of locally defined types?
    return is_type(l.peek().unwrap().content.as_ref().unwrap(), &ctx.typenames);
}

fn ns_follows(l: &mut Lexer, ctx: &Ctx) -> bool {
    if !l.more() {
        return false;
    }
    let t = l.peek().unwrap();
    return t.kind == "word" && ctx.has_ns(l.peek().unwrap().content.as_ref().unwrap());
}

// foo.bar where foo is a module
// or just bar
fn read_ns_id(l: &mut Lexer, ctx: &Ctx) -> Result<NsName, String> {
    let a = expect(l, "word", None)?;
    if ctx.has_ns(a.content.as_ref().unwrap())
        && l.peek_n(0).unwrap().kind == "."
        && l.peek_n(1).unwrap().kind == "word"
    {
        l.get();
        let b = l.get().unwrap();
        return Ok(NsName {
            namespace: a.content.unwrap(),
            name: b.content.unwrap(),
        });
    }
    return Ok(NsName {
        namespace: String::new(),
        name: a.content.unwrap(),
    });
}

fn expr_id(l: &mut Lexer, ctx: &Ctx) -> Result<Expression, String> {
    if !l.more() {
        return Err(String::from("id: unexpected end of input"));
    }
    if ns_follows(l, ctx) {
        return Ok(Expression::NsName(read_ns_id(l, ctx)?));
    }

    let next = l.get().unwrap();
    if next.kind == "word" {
        return Ok(Expression::Identifier(next.content.unwrap()));
    }
    if next.kind == "num" || next.kind == "string" || next.kind == "char" {
        l.unget(next);
        return Ok(Expression::Literal(parse_literal(l)?));
    }
    if next.kind == "(" {
        let e = parse_expr(l, 0, ctx);
        expect(l, ")", Some("parenthesized expression"))?;
        return e;
    }
    // Composite literal?
    if next.kind == "{" {
        l.unget(next);
        return Ok(Expression::CompositeLiteral(parse_composite_literal(
            l, ctx,
        )?));
    }
    return Err(format!("id: unexpected token {}", next));
}

fn is_op(token_type: &str) -> bool {
    let ops = [
        "+", "-", "*", "/", "=", "|", "&", "~", "^", "<", ">", "?", ":", "%", "+=", "-=", "*=",
        "/=", "%=", "&=", "^=", "|=", "++", "--", "->", ".", ">>", "<<", "<=", ">=", "&&", "||",
        "==", "!=", "<<=", ">>=",
    ];
    return ops.contains(&token_type);
}

pub fn operator_strength(op: &str) -> usize {
    // The lower the operator, the stronger it binds. So, for example,
    // a = 1, b = 2 will be parsed as (a = 1), (b = 2).
    let map = [
        /* 1 */ vec![","],
        /* 2 */
        vec![
            "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=",
        ],
        /* 3 */ vec!["||"],
        /* 4 */ vec!["&&"],
        /* 5 */ vec!["|"],
        /* 6 */ vec!["^"],
        /* 7 */ vec!["&"],
        /* 8 */ vec!["!=", "=="],
        /* 9 */ vec![">", "<", ">=", "<="],
        /* 10 */ vec!["<<", ">>"],
        /* 11 */ vec!["+", "-"],
        /* 12 */ vec!["*", "/", "%"],
        /* 13 */ vec!["prefix", "unary", "cast", "deref", "sizeof", "address"],
        /* 14 */ vec!["->", ".", "index", "call", "suffix"],
    ];
    for (i, ops) in map.iter().enumerate() {
        if ops.contains(&op) {
            return i + 1;
        }
    }
    panic!("unknown operator: '{}'", op);
}

fn is_postfix_op(token: &str) -> bool {
    let ops = [
        "++", "--", // "(", "["
    ];
    return ops.to_vec().contains(&token);
}

fn is_prefix_op(op: &str) -> bool {
    let ops = ["!", "--", "++", "*", "~", "&", "-"];
    return ops.to_vec().contains(&op);
}

fn is_type(name: &str, typenames: &Vec<String>) -> bool {
    let types = [
        "struct",
        "enum",
        "union",
        "void",
        "char",
        "short",
        "int",
        "long",
        "float",
        "double",
        "unsigned",
        "bool",
        "va_list",
        "FILE",
        "ptrdiff_t",
        "size_t",
        "wchar_t",
        "int8_t",
        "int16_t",
        "int32_t",
        "int64_t",
        "uint8_t",
        "uint16_t",
        "uint32_t",
        "uint64_t",
        "clock_t",
        "time_t",
        "fd_set",
        "socklen_t",
        "ssize_t",
    ];
    return types.to_vec().contains(&name)
        || typenames.to_vec().contains(&String::from(name))
        || name.ends_with("_t");
}

fn with_comment(comment: Option<&str>, message: String) -> String {
    match comment {
        Some(s) => format!("{}: {}", s, message),
        None => message.to_string(),
    }
}

fn expect(lexer: &mut Lexer, kind: &str, comment: Option<&str>) -> Result<Token, String> {
    if !lexer.more() {
        return Err(with_comment(
            comment,
            format!("expected '{}', got end of file", kind),
        ));
    }
    let next = lexer.peek().unwrap();
    if next.kind != kind {
        return Err(with_comment(
            comment,
            format!(
                "expected '{}', got '{}' at {}",
                kind,
                token_to_string(next),
                next.pos
            ),
        ));
    }
    return Ok(lexer.get().unwrap());
}

fn read_identifier(lexer: &mut Lexer) -> Result<String, String> {
    let identifier = String::from(expect(lexer, "word", None)?.content.unwrap());
    return Ok(identifier);
}

fn parse_typename(l: &mut Lexer, comment: Option<&str>, ctx: &Ctx) -> Result<Typename, String> {
    let is_const = l.eat("const");
    let name = read_ns_id(l, ctx)?;
    return Ok(Typename { is_const, name });
}

fn parse_anonymous_typeform(lexer: &mut Lexer, ctx: &Ctx) -> Result<AnonymousTypeform, String> {
    let type_name = parse_typename(lexer, None, ctx).unwrap();
    let mut ops: Vec<String> = Vec::new();
    while lexer.follows("*") {
        ops.push(lexer.get().unwrap().kind);
    }
    return Ok(AnonymousTypeform { type_name, ops });
}

fn parse_anonymous_parameters(l: &mut Lexer, ctx: &Ctx) -> Result<AnonymousParameters, String> {
    let mut params = AnonymousParameters {
        ellipsis: false,
        forms: Vec::new(),
    };
    expect(l, "(", Some("anonymous function parameters")).unwrap();
    if !l.follows(")") {
        params.forms.push(parse_anonymous_typeform(l, ctx).unwrap());
        while l.follows(",") {
            l.get();
            if l.follows("...") {
                l.get();
                params.ellipsis = true;
                break;
            }
            params.forms.push(parse_anonymous_typeform(l, ctx).unwrap());
        }
    }
    expect(l, ")", Some("anonymous function parameters"))?;
    return Ok(params);
}

fn parse_literal(l: &mut Lexer) -> Result<Literal, String> {
    let next = l.peek().unwrap();
    if next.kind == "string".to_string() {
        let value = l.get().unwrap().content.unwrap();
        return Ok(Literal {
            type_name: "string".to_string(),
            value,
        });
    }
    if next.kind == "num".to_string() {
        let value = l.get().unwrap().content.unwrap();
        return Ok(Literal {
            type_name: "num".to_string(),
            value,
        });
    }
    if next.kind == "char".to_string() {
        let value = l.get().unwrap().content.unwrap();
        return Ok(Literal {
            type_name: "char".to_string(),
            value,
        });
    }
    if next.kind == "word" && next.content.as_ref().unwrap() == "NULL" {
        l.get();
        return Ok(Literal {
            type_name: "null".to_string(),
            value: "NULL".to_string(),
        });
    }
    return Err(format!("literal expected, got {}", l.peek().unwrap()));
}

fn parse_enum(lexer: &mut Lexer, is_pub: bool, ctx: &Ctx) -> Result<ModuleObject, String> {
    let mut members: Vec<EnumItem> = Vec::new();
    expect(lexer, "enum", Some("enum definition"))?;
    expect(lexer, "{", Some("enum definition"))?;
    loop {
        let id = read_identifier(lexer)?;
        let value: Option<Expression> = if lexer.eat("=") {
            Some(parse_expr(lexer, 0, ctx)?)
        } else {
            None
        };
        members.push(EnumItem { id, value });
        if lexer.eat(",") {
            continue;
        } else {
            break;
        }
    }
    expect(lexer, "}", Some("enum definition"))?;
    expect(lexer, ";", Some("enum definition"))?;
    return Ok(ModuleObject::Enum { is_pub, members });
}

fn parse_composite_literal(l: &mut Lexer, ctx: &Ctx) -> Result<CompositeLiteral, String> {
    let mut result = CompositeLiteral {
        entries: Vec::new(),
    };
    expect(l, "{", Some("composite literal"))?;
    loop {
        if l.peek_skipping_comments().unwrap().kind == "}" {
            break;
        }
        result.entries.push(parse_composite_literal_entry(l, ctx)?);
        if l.peek().unwrap().kind == "," {
            l.get();
        } else {
            break;
        }
    }
    expect(l, "}", Some("struct literal"))?;
    return Ok(result);
}

fn parse_composite_literal_entry(
    l: &mut Lexer,
    ctx: &Ctx,
) -> Result<CompositeLiteralEntry, String> {
    if l.peek().unwrap().kind == "." {
        expect(l, ".", Some("struct literal member"))?;
        let key = Expression::Identifier(read_identifier(l)?);
        expect(l, "=", Some("struct literal member"))?;
        let value = parse_expr(l, 0, ctx)?;
        return Ok(CompositeLiteralEntry {
            is_index: false,
            key: Some(key),
            value,
        });
    }
    if l.follows("[") {
        l.get();
        let key = parse_expr(l, 0, ctx)?;
        expect(l, "]", Some("array literal index"))?;
        expect(l, "=", Some("array literal index"))?;
        let value = parse_expr(l, 0, ctx)?;
        return Ok(CompositeLiteralEntry {
            is_index: true,
            key: Some(key),
            value,
        });
    }
    return Ok(CompositeLiteralEntry {
        is_index: false,
        key: None,
        value: parse_expr(l, 0, ctx)?,
    });
}

fn parse_sizeof(l: &mut Lexer, ctx: &Ctx) -> Result<Expression, String> {
    expect(l, "sizeof", None)?;
    expect(l, "(", None)?;
    let argument = if type_follows(l, ctx) {
        SizeofArgument::Typename(parse_typename(l, None, ctx)?)
    } else {
        SizeofArgument::Expression(parse_expr(l, 0, ctx).unwrap())
    };
    expect(l, ")", None)?;
    return Ok(Expression::Sizeof {
        argument: Box::new(argument),
    });
}

fn parse_function_call(
    l: &mut Lexer,
    ctx: &Ctx,
    function_name: Expression,
) -> Result<Expression, String> {
    let mut arguments: Vec<Expression> = Vec::new();
    expect(l, "(", None)?;
    if l.more() && l.peek().unwrap().kind != ")" {
        arguments.push(parse_expr(l, 0, ctx)?);
        while l.follows(",") {
            l.get();
            arguments.push(parse_expr(l, 0, ctx)?);
        }
    }
    expect(l, ")", None)?;
    return Ok(Expression::FunctionCall {
        function: Box::new(function_name),
        arguments: arguments,
    });
}

fn parse_while(lexer: &mut Lexer, ctx: &Ctx) -> Result<Statement, String> {
    expect(lexer, "while", None)?;
    expect(lexer, "(", None)?;
    let condition = parse_expr(lexer, 0, ctx)?;
    expect(lexer, ")", None)?;
    let body = parse_statements_block(lexer, ctx)?;
    return Ok(Statement::While { condition, body });
}

fn parse_statements_block(lexer: &mut Lexer, ctx: &Ctx) -> Result<Body, String> {
    let mut statements: Vec<Statement> = Vec::new();
    if lexer.follows("{") {
        expect(lexer, "{", None)?;
        while !lexer.follows("}") {
            let s = parse_statement(lexer, ctx)?;
            statements.push(s);
        }
        expect(lexer, "}", None)?;
    } else {
        statements.push(parse_statement(lexer, ctx)?);
    }
    return Ok(Body { statements });
}

fn parse_statement(l: &mut Lexer, ctx: &Ctx) -> Result<Statement, String> {
    let next = l.peek().unwrap();
    if type_follows(l, ctx) || next.kind == "const" {
        return parse_variable_declaration(l, ctx);
    }
    return match next.kind.as_str() {
        "if" => parse_if(l, ctx),
        "for" => parse_for(l, ctx),
        "while" => parse_while(l, ctx),
        "return" => parse_return(l, ctx),
        "switch" => parse_switch(l, ctx),
        _ => {
            let expr = parse_expr(l, 0, ctx)?;
            expect(l, ";", Some("parsing statement"))?;
            return Ok(Statement::Expression(expr));
        }
    };
}

fn parse_variable_declaration(l: &mut Lexer, ctx: &Ctx) -> Result<Statement, String> {
    let type_name = parse_typename(l, None, ctx)?;
    let mut forms = vec![];
    let mut values = vec![];
    loop {
        forms.push(parse_form(l, ctx)?);
        if l.eat("=") {
            values.push(Some(parse_expr(l, 0, ctx)?));
        } else {
            values.push(None)
        };
        if !l.eat(",") {
            break;
        }
    }
    expect(l, ";", None)?;
    return Ok(Statement::VariableDeclaration {
        type_name,
        forms,
        values,
    });
}

fn parse_return(lexer: &mut Lexer, ctx: &Ctx) -> Result<Statement, String> {
    expect(lexer, "return", None)?;
    if lexer.peek().unwrap().kind == ";" {
        lexer.get();
        return Ok(Statement::Return { expression: None });
    }
    let expression = parse_expr(lexer, 0, ctx)?;
    expect(lexer, ";", None)?;
    return Ok(Statement::Return {
        expression: Some(expression),
    });
}

fn parse_form(lexer: &mut Lexer, ctx: &Ctx) -> Result<Form, String> {
    // *argv[]
    // linechars[]
    // buf[SIZE * 2]
    let mut node = Form {
        stars: String::new(),
        name: String::new(),
        indexes: vec![],
    };

    while lexer.follows("*") {
        node.stars += &lexer.get().unwrap().kind;
    }

    match expect(lexer, "word", None)?.content {
        Some(x) => {
            node.name = String::from(x);
        }
        None => {
            return Err(String::from("missing word content"));
        }
    }

    while lexer.follows("[") {
        lexer.get().unwrap();
        let e: Option<Expression>;
        if lexer.more() && lexer.peek().unwrap().kind != "]" {
            e = Some(parse_expr(lexer, 0, ctx)?);
        } else {
            e = None;
        }
        node.indexes.push(e);
        expect(lexer, "]", None)?;
    }
    return Ok(node);
}

fn parse_if(lexer: &mut Lexer, ctx: &Ctx) -> Result<Statement, String> {
    let condition;
    let body;
    let mut else_body = None;

    expect(lexer, "if", Some("if statement"))?;
    expect(lexer, "(", Some("if statement"))?;
    condition = parse_expr(lexer, 0, ctx)?;
    expect(lexer, ")", Some("if statement"))?;
    body = parse_statements_block(lexer, ctx)?;
    if lexer.follows("else") {
        lexer.get();
        else_body = Some(parse_statements_block(lexer, ctx)?);
    }
    return Ok(Statement::If {
        condition,
        body,
        else_body,
    });
}

fn parse_for(l: &mut Lexer, ctx: &Ctx) -> Result<Statement, String> {
    expect(l, "for", None)?;
    expect(l, "(", None)?;

    let init: ForInit;
    if l.peek().unwrap().kind == "word"
        && is_type(&l.peek().unwrap().content.as_ref().unwrap(), &ctx.typenames)
    {
        let type_name = parse_typename(l, None, ctx)?;
        let form = parse_form(l, ctx)?;
        expect(l, "=", None)?;
        let value = parse_expr(l, 0, ctx)?;
        init = ForInit::LoopCounterDeclaration {
            type_name,
            form,
            value,
        };
    } else {
        init = ForInit::Expression(parse_expr(l, 0, ctx)?);
    }

    expect(l, ";", None)?;
    let condition = parse_expr(l, 0, ctx)?;
    expect(l, ";", None)?;
    let action = parse_expr(l, 0, ctx)?;
    expect(l, ")", None)?;
    let body = parse_statements_block(l, ctx)?;

    return Ok(Statement::For {
        init,
        condition,
        action,
        body,
    });
}

fn parse_switch(lexer: &mut Lexer, ctx: &Ctx) -> Result<Statement, String> {
    expect(lexer, "switch", None)?;
    expect(lexer, "(", None)?;
    let value = parse_expr(lexer, 0, ctx)?;
    let mut cases: Vec<SwitchCase> = vec![];
    expect(lexer, ")", None)?;
    expect(lexer, "{", None)?;
    while lexer.follows("case") {
        expect(lexer, "case", None)?;
        let case_value = if lexer.follows("word") {
            SwitchCaseValue::Identifier(read_identifier(lexer)?)
        } else {
            SwitchCaseValue::Literal(parse_literal(lexer)?)
        };
        expect(lexer, ":", None)?;
        let until = ["case", "break", "default", "}"];
        let mut statements: Vec<Statement> = vec![];
        while lexer.more() && !until.contains(&lexer.peek().unwrap().kind.as_str()) {
            statements.push(parse_statement(lexer, ctx)?);
        }
        cases.push(SwitchCase {
            value: case_value,
            body: Body { statements },
        });
    }
    let mut default: Option<Body> = None;
    if lexer.follows("default") {
        expect(lexer, "default", None)?;
        expect(lexer, ":", None)?;
        let mut def: Vec<Statement> = vec![];
        while lexer.more() && lexer.peek().unwrap().kind != "}" {
            def.push(parse_statement(lexer, ctx)?);
        }
        default = Some(Body { statements: def });
    }
    expect(lexer, "}", None)?;
    return Ok(Statement::Switch {
        value,
        cases,
        default,
    });
}

fn parse_function_declaration(
    lexer: &mut Lexer,
    is_pub: bool,
    type_name: Typename,
    form: Form,
    ctx: &Ctx,
) -> Result<ModuleObject, String> {
    // void cuespl {WE ARE HERE} (cue_t *c, mp3file *m) {...}

    let mut parameters: Vec<TypeAndForms> = vec![];
    let mut variadic = false;

    expect(lexer, "(", None)?;
    if !lexer.follows(")") {
        parameters.push(parse_function_parameter(lexer, ctx)?);
        while lexer.follows(",") {
            lexer.get();
            if lexer.follows("...") {
                lexer.get();
                variadic = true;
                break;
            }
            parameters.push(parse_function_parameter(lexer, ctx)?);
        }
    }
    expect(lexer, ")", Some("parse_function_declaration"))?;
    let body = parse_statements_block(lexer, ctx)?;
    return Ok(ModuleObject::FunctionDeclaration(FunctionDeclaration {
        is_pub,
        type_name,
        form,
        parameters: FunctionParameters {
            list: parameters,
            variadic,
        },
        body,
    }));
}

fn parse_function_parameter(lexer: &mut Lexer, ctx: &Ctx) -> Result<TypeAndForms, String> {
    let mut forms: Vec<Form> = vec![];
    let type_name = parse_typename(lexer, None, ctx)?;
    forms.push(parse_form(lexer, ctx)?);
    while lexer.follows(",")
        && lexer.peek_n(1).unwrap().kind != "..."
        && lexer.peek_n(1).unwrap().kind != "const"
        && !(lexer.peek_n(1).unwrap().kind == "word"
            && is_type(
                lexer.peek_n(1).unwrap().content.as_ref().unwrap(),
                &ctx.typenames,
            ))
    {
        lexer.get();
        forms.push(parse_form(lexer, ctx)?);
    }
    return Ok(TypeAndForms { type_name, forms });
}

fn parse_union(lexer: &mut Lexer, ctx: &Ctx) -> Result<Union, String> {
    let mut fields: Vec<UnionField> = vec![];
    expect(lexer, "union", None)?;
    expect(lexer, "{", None)?;
    while !lexer.follows("}") {
        let type_name = parse_typename(lexer, None, ctx)?;
        let form = parse_form(lexer, ctx)?;
        expect(lexer, ";", None)?;
        fields.push(UnionField { type_name, form });
    }
    expect(lexer, "}", None)?;
    let form = parse_form(lexer, ctx)?;
    expect(lexer, ";", None)?;
    return Ok(Union { form, fields });
}

fn parse_typedef(is_pub: bool, l: &mut Lexer, ctx: &Ctx) -> Result<ModuleObject, String> {
    expect(l, "typedef", None)?;

    // typedef {int hour, minute, second} time_t;
    if l.follows("{") {
        // struct body
        let mut fields: Vec<StructEntry> = Vec::new();
        expect(l, "{", Some("struct type definition"))?;
        while l.more() {
            match l.peek().unwrap().kind.as_str() {
                "}" => break,
                "union" => fields.push(StructEntry::Union(parse_union(l, ctx)?)),
                _ => fields.push(StructEntry::Plain(parse_type_and_forms(l, ctx)?)),
            }
        }
        expect(l, "}", None)?;

        // type name
        let name = read_identifier(l)?;
        expect(l, ";", Some("typedef"))?;
        return Ok(ModuleObject::StructTypedef(StructTypedef {
            is_pub,
            fields,
            name,
        }));
    }

    // typedef struct foo foo_t;
    if l.eat("struct") {
        let struct_name = expect(l, "word", None)?.content.unwrap();
        let type_alias = expect(l, "word", None)?.content.unwrap();
        expect(l, ";", Some("typedef"))?;
        return Ok(ModuleObject::StructAliasTypedef {
            is_pub,
            struct_name,
            type_alias,
        });
    }

    // typedef void *thr_func(void *)
    // typedef foo bar;
    // typedef uint32_t md5sum_t[4];
    let typename = parse_typename(l, Some("typedef"), ctx)?;
    let mut stars: usize = 0;
    while l.follows("*") {
        l.get();
        stars += 1;
    }
    let alias = read_identifier(l)?;

    let params = if l.follows("(") {
        Some(parse_anonymous_parameters(l, ctx)?)
    } else {
        None
    };
    let mut size: usize = 0;
    if l.follows("[") {
        l.get();
        size = expect(l, "num", None)
            .unwrap()
            .content
            .unwrap()
            .parse()
            .unwrap();
        expect(l, "]", None)?;
    }
    expect(l, ";", Some("typedef"))?;
    return Ok(ModuleObject::Typedef(Typedef {
        is_pub,
        type_name: typename,
        dereference_count: stars,
        function_parameters: params,
        array_size: size,
        alias,
    }));
}

fn parse_type_and_forms(lexer: &mut Lexer, ctx: &Ctx) -> Result<TypeAndForms, String> {
    if lexer.follows("struct") {
        return Err("can't parse nested structs, please consider a typedef".to_string());
    }

    let type_name = parse_typename(lexer, None, ctx)?;

    let mut forms: Vec<Form> = vec![];
    forms.push(parse_form(lexer, ctx)?);
    while lexer.follows(",") {
        lexer.get();
        forms.push(parse_form(lexer, ctx)?);
    }

    expect(lexer, ";", None)?;
    return Ok(TypeAndForms { type_name, forms });
}

fn token_to_string(token: &Token) -> String {
    if token.content.is_none() {
        return format!("[{}]", token.kind);
    }

    let n = 40;
    let mut c: String;
    if token.content.as_ref().unwrap().len() > n {
        c = token.content.as_ref().unwrap()[0..(n - 3)].to_string() + "...";
    } else {
        c = token.content.as_ref().unwrap().to_string();
    }
    c = c.replace("\r", "\\r");
    c = c.replace("\n", "\\n");
    c = c.replace("\t", "\\t");
    return format!("[{}, {}]", token.kind, c);
}
