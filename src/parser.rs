use crate::build::Ctx;
use crate::lexer::{Lexer, Token};
use crate::{c, nodes::*};

pub struct Error {
    pub message: String,
    pub pos: String,
}

pub fn parse_module(l: &mut Lexer, ctx: &Ctx) -> Result<Module, Error> {
    let mut module_objects: Vec<ModuleObject> = vec![];
    while l.more() {
        match l.peek().unwrap().kind.as_str() {
            "import" => {
                // The context already contains all parsed and resolved imports,
                // here this node is added only for completeness, to allow the
                // source code checkers look at them.
                let t = l.get().unwrap();
                module_objects.push(ModuleObject::Import(ImportNode {
                    specified_path: t.content.clone(),
                    pos: t.pos.clone(),
                }))
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

fn parse_compat_macro(lexer: &mut Lexer) -> Result<ModuleObject, Error> {
    let tok = expect(lexer, "macro", None)?;
    let content = tok.content.clone();
    let pos = content.find(" ");
    if pos.is_none() {
        return Err(Error {
            message: format!("can't get macro name from '{}'", content),
            pos: tok.pos,
        });
    }
    let (name, value) = content.split_at(pos.unwrap());

    return Ok(ModuleObject::Macro {
        name: name[1..].to_string(),
        value: value.to_string(),
    });
}

fn parse_module_object(l: &mut Lexer, ctx: &Ctx) -> Result<ModuleObject, Error> {
    let mut is_pub = false;
    let pos = l.peek().unwrap().pos.clone();
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
    let type_name = parse_typename(l, ctx)?;
    let form = parse_form(l, ctx)?;
    if l.peek().unwrap().kind == "(" {
        return parse_function_declaration(l, is_pub, type_name, form, ctx);
    }

    expect(l, "=", Some("module variable"))?;
    if is_pub {
        return Err(Error {
            message: "module variables can't be exported".to_string(),
            pos,
        });
    }
    let value = parse_expr(l, 0, ctx)?;
    expect(l, ";", Some("module variable declaration"))?;
    return Ok(ModuleObject::ModuleVariable {
        type_name,
        form,
        value,
    });
}

fn parse_expr(l: &mut Lexer, strength: usize, ctx: &Ctx) -> Result<Expression, Error> {
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

fn parse_seq_expr(l: &mut Lexer, strength: usize, ctx: &Ctx) -> Result<Expression, Error> {
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

fn parse_prefix_expr(l: &mut Lexer, ctx: &Ctx) -> Result<Expression, Error> {
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
        && is_type(l.peek().unwrap().content.as_str(), &ctx.typenames)
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

fn base(l: &mut Lexer, ctx: &Ctx) -> Result<Expression, Error> {
    // println!("      base(): {:?}", l.peek());
    let mut r = read_expression_atom(l, ctx)?;
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
            r = Expression::FieldAccess {
                op: l.get().unwrap().kind,
                target: Box::new(r),
                field_name: read_identifier(l)?,
            };
            continue;
        }

        break;
    }
    return Ok(r);
}

fn type_follows(l: &mut Lexer, ctx: &Ctx) -> bool {
    if !l.peek().is_some() {
        return false;
    }
    if l.peek().unwrap().kind == "const" {
        return true;
    }
    if l.peek().unwrap().kind != "word" {
        return false;
    }
    // <mod>.<name> where <mod> is any imported module that has exported <name>?
    match l.peekn(3) {
        Some(three) => {
            if three[1].kind == "." && three[2].kind == "word" {
                let a = three[0];
                let c = three[2];
                let pos = ctx.deps.iter().position(|x| x.ns == *a.content);
                if pos.is_some() && ctx.deps[pos.unwrap()].typenames.contains(&c.content) {
                    return true;
                }
            }
        }
        None => {}
    }
    // any of locally defined types?
    return is_type(&l.peek().unwrap().content, &ctx.typenames);
}

fn ns_follows(l: &mut Lexer, ctx: &Ctx) -> bool {
    if !l.more() {
        return false;
    }
    let t = l.peek().unwrap();
    if t.kind != "word" {
        return false;
    }
    return ctx.has_ns(&l.peek().unwrap().content);
}

// foo.bar where foo is a module
// or just bar
fn read_ns_id(l: &mut Lexer, ctx: &Ctx) -> Result<NsName, Error> {
    let a = expect(l, "word", None)?;
    let name = &a.content;
    if ctx.has_ns(name) {
        match l.peekn(2) {
            Some(two) => {
                if two[0].kind == "." && two[1].kind == "word" {
                    l.get();
                    let b = l.get().unwrap();
                    return Ok(NsName {
                        namespace: a.content,
                        name: b.content,
                        pos: a.pos,
                    });
                }
            }
            None => {}
        }
    }
    return Ok(NsName {
        namespace: String::new(),
        name: a.content,
        pos: a.pos,
    });
}

fn read_expression_atom(l: &mut Lexer, ctx: &Ctx) -> Result<Expression, Error> {
    if !l.more() {
        return Err(Error {
            message: String::from("id: unexpected end of input"),
            pos: String::from("EOF"),
        });
    }
    if ns_follows(l, ctx) {
        return Ok(Expression::NsName(read_ns_id(l, ctx)?));
    }

    let next = l.get().unwrap();
    return match next.kind.as_str() {
        "word" => Ok(Expression::Identifier(Identifier {
            name: next.content,
            pos: next.pos,
        })),
        "num" | "string" | "char" => {
            l.unget(next);
            Ok(Expression::Literal(parse_literal(l)?))
        }
        "(" => {
            let e = parse_expr(l, 0, ctx);
            expect(l, ")", Some("parenthesized expression"))?;
            e
        }
        "{" => {
            l.unget(next);
            Ok(Expression::CompositeLiteral(parse_composite_literal(
                l, ctx,
            )?))
        }
        _ => Err(Error {
            message: format!("id: unexpected token {}", next),
            pos: next.pos,
        }),
    };
}

fn is_op(token_type: &str) -> bool {
    let ops = [
        "+", "-", "*", "/", "=", "|", "&", "~", "^", "<", ">", ":", "%", "+=", "-=", "*=", "/=",
        "%=", "&=", "^=", "|=", "++", "--", "->", ".", ">>", "<<", "<=", ">=", "&&", "||", "==",
        "!=", "<<=", ">>=",
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
    let keywords = [
        "struct", "enum", "union",
        // "fd_set",
        // "socklen_t",
        // "ssize_t",
    ];

    return c::CTYPES.contains(&name)
        || keywords.to_vec().contains(&name)
        || typenames.to_vec().contains(&String::from(name));
    // || name.ends_with("_t");
}

fn with_comment(comment: Option<&str>, message: String) -> String {
    match comment {
        Some(s) => format!("{}: {}", s, message),
        None => message.to_string(),
    }
}

fn expect(l: &mut Lexer, kind: &str, comment: Option<&str>) -> Result<Token, Error> {
    if !l.more() {
        return Err(Error {
            message: with_comment(comment, format!("expected '{}', got end of file", kind)),
            pos: String::from("EOF"),
        });
    }
    let next = l.peek().unwrap();
    if next.kind != kind {
        return Err(Error {
            message: with_comment(
                comment,
                format!("expected '{}', got '{}'", kind, token_to_string(next),),
            ),
            pos: next.pos.clone(),
        });
    }
    return Ok(l.get().unwrap());
}

fn read_identifier(lexer: &mut Lexer) -> Result<Identifier, Error> {
    let tok = expect(lexer, "word", None)?;
    let name = String::from(tok.content);
    return Ok(Identifier { name, pos: tok.pos });
}

fn parse_typename(l: &mut Lexer, ctx: &Ctx) -> Result<Typename, Error> {
    let is_const = l.eat("const");
    let name = read_ns_id(l, ctx)?;
    return Ok(Typename { is_const, name });
}

fn parse_anonymous_typeform(lexer: &mut Lexer, ctx: &Ctx) -> Result<AnonymousTypeform, Error> {
    let type_name = parse_typename(lexer, ctx)?;
    let mut ops: Vec<String> = Vec::new();
    while lexer.follows("*") {
        ops.push(lexer.get().unwrap().kind);
    }
    return Ok(AnonymousTypeform { type_name, ops });
}

fn parse_anonymous_parameters(l: &mut Lexer, ctx: &Ctx) -> Result<AnonymousParameters, Error> {
    let mut params = AnonymousParameters {
        ellipsis: false,
        forms: Vec::new(),
    };
    expect(l, "(", Some("anonymous function parameters"))?;
    if !l.follows(")") {
        params.forms.push(parse_anonymous_typeform(l, ctx)?);
        while l.follows(",") {
            l.get();
            if l.follows("...") {
                l.get();
                params.ellipsis = true;
                break;
            }
            params.forms.push(parse_anonymous_typeform(l, ctx)?);
        }
    }
    expect(l, ")", Some("anonymous function parameters"))?;
    return Ok(params);
}

fn parse_literal(l: &mut Lexer) -> Result<Literal, Error> {
    let next = l.peek().unwrap();
    if next.kind == "string".to_string() {
        let value = l.get().unwrap().content;
        return Ok(Literal::String(value));
    }
    if next.kind == "num".to_string() {
        let value = l.get().unwrap().content;
        return Ok(Literal::Number(value));
    }
    if next.kind == "char".to_string() {
        let value = l.get().unwrap().content;
        return Ok(Literal::Char(value));
    }
    if next.kind == "word" && next.content == "NULL" {
        l.get();
        return Ok(Literal::Null);
    }
    return Err(Error {
        message: format!("literal expected, got {}", l.peek().unwrap()),
        pos: l.peek().unwrap().pos.clone(),
    });
}

fn parse_enum(lexer: &mut Lexer, is_pub: bool, ctx: &Ctx) -> Result<ModuleObject, Error> {
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

fn parse_composite_literal(l: &mut Lexer, ctx: &Ctx) -> Result<CompositeLiteral, Error> {
    let mut result = CompositeLiteral {
        entries: Vec::new(),
    };
    expect(l, "{", Some("composite literal"))?;
    loop {
        if l.peek().unwrap().kind == "}" {
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

fn parse_composite_literal_entry(l: &mut Lexer, ctx: &Ctx) -> Result<CompositeLiteralEntry, Error> {
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

fn parse_sizeof(l: &mut Lexer, ctx: &Ctx) -> Result<Expression, Error> {
    expect(l, "sizeof", None)?;
    expect(l, "(", None)?;
    let argument = if type_follows(l, ctx) {
        SizeofArgument::Typename(parse_typename(l, ctx)?)
    } else {
        SizeofArgument::Expression(parse_expr(l, 0, ctx)?)
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
) -> Result<Expression, Error> {
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

fn parse_while(l: &mut Lexer, ctx: &Ctx) -> Result<Statement, Error> {
    expect(l, "while", None)?;
    expect(l, "(", None)?;
    let condition = parse_expr(l, 0, ctx)?;
    expect(l, ")", None)?;
    let body = parse_statements_block(l, ctx)?;
    return Ok(Statement::While { condition, body });
}

fn parse_statement(l: &mut Lexer, ctx: &Ctx) -> Result<Statement, Error> {
    if !l.more() {
        return Err(Error {
            message: String::from("reached end of file while parsing statement"),
            pos: String::from("EOF"),
        });
    }
    if type_follows(l, ctx) {
        return parse_variable_declaration(l, ctx);
    }

    let next = l.peek().unwrap();
    return match next.kind.as_str() {
        "for" => parse_for(l, ctx),
        "if" => parse_if(l, ctx),
        "panic" => parse_panic(l, ctx),
        "return" => parse_return(l, ctx),
        "switch" => parse_switch(l, ctx),
        "while" => parse_while(l, ctx),
        _ => {
            let expr = parse_expr(l, 0, ctx)?;
            expect(l, ";", Some("parsing statement"))?;
            return Ok(Statement::Expression(expr));
        }
    };
}

fn parse_panic(l: &mut Lexer, ctx: &Ctx) -> Result<Statement, Error> {
    let mut arguments: Vec<Expression> = Vec::new();
    let p = expect(l, "panic", None)?;
    expect(l, "(", Some("panic"))?;
    if l.more() && l.peek().unwrap().kind != ")" {
        arguments.push(parse_expr(l, 0, ctx)?);
        while l.follows(",") {
            l.get();
            arguments.push(parse_expr(l, 0, ctx)?);
        }
    }
    expect(l, ")", Some("panic"))?;
    expect(l, ";", Some("panic"))?;
    return Ok(Statement::Panic {
        arguments,
        pos: format!("{}:{}", ctx.path, p.pos),
    });
}

fn parse_variable_declaration(l: &mut Lexer, ctx: &Ctx) -> Result<Statement, Error> {
    let pos = l.peek().unwrap().pos.clone();
    let type_name = parse_typename(l, ctx)?;
    let form = parse_form(l, ctx)?;
    let value = if l.eat("=") {
        Some(parse_expr(l, 0, ctx)?)
    } else {
        None
    };
    expect(l, ";", None)?;
    return Ok(Statement::VariableDeclaration {
        pos,
        type_name,
        form,
        value,
    });
}

fn parse_return(l: &mut Lexer, ctx: &Ctx) -> Result<Statement, Error> {
    expect(l, "return", None)?;
    if l.peek().unwrap().kind == ";" {
        l.get();
        return Ok(Statement::Return { expression: None });
    }
    let expression = parse_expr(l, 0, ctx)?;
    expect(l, ";", None)?;
    return Ok(Statement::Return {
        expression: Some(expression),
    });
}

fn parse_form(lexer: &mut Lexer, ctx: &Ctx) -> Result<Form, Error> {
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

    let tok = expect(lexer, "word", None)?;
    if tok.content == "" {
        return Err(Error {
            message: String::from("missing word content"),
            pos: tok.pos,
        });
    }
    node.name = tok.content.clone();

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

fn parse_if(lexer: &mut Lexer, ctx: &Ctx) -> Result<Statement, Error> {
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

fn parse_for(l: &mut Lexer, ctx: &Ctx) -> Result<Statement, Error> {
    expect(l, "for", None)?;
    expect(l, "(", None)?;

    let init: ForInit;
    if l.peek().unwrap().kind == "word" && is_type(&l.peek().unwrap().content, &ctx.typenames) {
        let type_name = parse_typename(l, ctx)?;
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

fn parse_switch(l: &mut Lexer, ctx: &Ctx) -> Result<Statement, Error> {
    expect(l, "switch", None)?;
    expect(l, "(", None)?;
    let value = parse_expr(l, 0, ctx)?;
    let mut cases: Vec<SwitchCase> = vec![];
    expect(l, ")", None)?;
    expect(l, "{", None)?;
    while l.follows("case") {
        cases.push(read_switch_case(l, ctx)?);
    }
    let mut default: Option<Body> = None;
    if l.follows("default") {
        expect(l, "default", None)?;
        expect(l, ":", None)?;
        default = Some(read_body(l, ctx)?);
    }
    expect(l, "}", None)?;
    return Ok(Statement::Switch {
        value,
        cases,
        default_case: default,
    });
}

fn read_switch_case(l: &mut Lexer, ctx: &Ctx) -> Result<SwitchCase, Error> {
    expect(l, "case", None)?;
    let mut values = Vec::new();
    loop {
        let case_value = if l.follows("word") {
            SwitchCaseValue::Identifier(read_ns_id(l, ctx)?)
        } else {
            SwitchCaseValue::Literal(parse_literal(l)?)
        };
        values.push(case_value);
        if l.follows(",") {
            l.get().unwrap();
        } else {
            break;
        }
    }
    expect(l, ":", None)?;
    let body = read_body(l, ctx)?;
    return Ok(SwitchCase { values, body });
}

fn parse_function_declaration(
    l: &mut Lexer,
    is_pub: bool,
    type_name: Typename,
    form: Form,
    ctx: &Ctx,
) -> Result<ModuleObject, Error> {
    // void cuespl {WE ARE HERE} (cue_t *c, mp3file *m) {...}

    let mut parameters: Vec<TypeAndForms> = vec![];
    let mut variadic = false;

    // (const int a,b, float c, ...)
    expect(l, "(", None)?;
    while type_follows(l, ctx) {
        parameters.push(parse_function_parameter(l, ctx)?);
        if !l.eat(",") {
            break;
        }
    }
    if l.follows("...") {
        l.get();
        variadic = true;
    }
    expect(l, ")", Some("parse_function_declaration"))?;

    let body = parse_statements_block(l, ctx)?;
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

fn parse_function_parameter(l: &mut Lexer, ctx: &Ctx) -> Result<TypeAndForms, Error> {
    // int a,b
    // const char *s
    let type_name = parse_typename(l, ctx)?;
    let mut forms: Vec<Form> = Vec::new();
    forms.push(parse_form(l, ctx)?);
    while l.follows(",") {
        let t = l.get().unwrap();
        if type_follows(l, ctx) || l.follows("...") {
            l.unget(t);
            break;
        }
        forms.push(parse_form(l, ctx)?);
    }
    return Ok(TypeAndForms { type_name, forms });
}

fn parse_statements_block(l: &mut Lexer, ctx: &Ctx) -> Result<Body, Error> {
    if l.follows("{") {
        return read_body(l, ctx);
    }
    let statements: Vec<Statement> = vec![parse_statement(l, ctx)?];
    return Ok(Body { statements });
}

fn read_body(l: &mut Lexer, ctx: &Ctx) -> Result<Body, Error> {
    let mut statements: Vec<Statement> = Vec::new();
    expect(l, "{", None)?;
    while !l.follows("}") {
        let s = parse_statement(l, ctx)?;
        statements.push(s);
    }
    expect(l, "}", None)?;
    return Ok(Body { statements });
}

fn parse_union(l: &mut Lexer, ctx: &Ctx) -> Result<Union, Error> {
    let mut fields: Vec<UnionField> = vec![];
    expect(l, "union", None)?;
    expect(l, "{", None)?;
    while !l.follows("}") {
        let type_name = parse_typename(l, ctx)?;
        let form = parse_form(l, ctx)?;
        expect(l, ";", None)?;
        fields.push(UnionField { type_name, form });
    }
    expect(l, "}", None)?;
    let form = parse_form(l, ctx)?;
    expect(l, ";", None)?;
    return Ok(Union { form, fields });
}

fn parse_typedef(is_pub: bool, l: &mut Lexer, ctx: &Ctx) -> Result<ModuleObject, Error> {
    let pos = l.peek().unwrap().pos.clone();
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
            pos,
            is_pub,
            fields,
            name,
        }));
    }

    // typedef struct foo foo_t;
    if l.eat("struct") {
        let struct_name = expect(l, "word", None)?.content;
        let type_alias = expect(l, "word", None)?.content;
        expect(l, ";", Some("typedef"))?;
        return Ok(ModuleObject::StructAliasTypedef {
            pos,
            is_pub,
            struct_name,
            type_alias,
        });
    }

    // typedef void *thr_func(void *)
    // typedef foo bar;
    // typedef uint32_t md5sum_t[4];
    let typename = parse_typename(l, ctx)?;
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
        let num = expect(l, "num", None)?;
        size = num.content.parse().unwrap();
        expect(l, "]", None)?;
    }
    expect(l, ";", Some("typedef"))?;
    return Ok(ModuleObject::Typedef(Typedef {
        pos,
        is_pub,
        type_name: typename,
        dereference_count: stars,
        function_parameters: params,
        array_size: size,
        alias,
    }));
}

fn parse_type_and_forms(lexer: &mut Lexer, ctx: &Ctx) -> Result<TypeAndForms, Error> {
    if lexer.follows("struct") {
        return Err(Error {
            message: "Unexpected struct".to_string(),
            pos: lexer.peek().unwrap().pos.clone(),
        });
    }

    let type_name = parse_typename(lexer, ctx)?;

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
    if token.content == "" {
        return format!("[{}]", token.kind);
    }

    let n = 40;
    let mut c: String;
    if token.content.len() > n {
        c = token.content[0..(n - 3)].to_string() + "...";
    } else {
        c = token.content.to_string();
    }
    c = c.replace("\r", "\\r");
    c = c.replace("\n", "\\n");
    c = c.replace("\t", "\\t");
    return format!("[{}, {}]", token.kind, c);
}
