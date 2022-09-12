#[cfg(test)]
use crate::lexer::for_string;
use crate::lexer::{for_file, Lexer, Token};
use crate::nodes::*;
use std::env;
use std::path::Path;

struct Ctx {
    typenames: Vec<String>,
    modnames: Vec<String>,
}

#[test]
fn test_expressions_parser() {
    let ctx = Ctx {
        typenames: vec!["foo_t".to_string()],
        modnames: vec!["opt".to_string()],
    };
    let cases: Vec<&str> = vec![
        "1",
        "x",
        "a + b + c",
        "a + b*c",
        "f(x)",
        "f(a+b)",
        "&a + *b->c++",
        "a * (b + c)",
        "12 * sizeof(int)",
        "242 * !(foo_t) foo;",
        "a + b * !c",
        "a + --c",
        "pos.usec = next->pos_usec",
        "a += 1",
        "(f()++).usec += 1",
        "x = -1 + -1",
        "(!arr_pushi(a, i))",
        "putch(ch, &(c->out))",
        "sizeof(b->data)",
        "a = {'a', 'b', 'c'}",
        "opt.opt()",
        "a = \"abc\"",
        "c = 'a'",
        "out = { .r = 1 }",
    ];
    for c in cases {
        println!("-----------{}------------", c);
        let mut l = for_string(c.to_string()).unwrap();
        let e = expr(&mut l, 0, &ctx).unwrap();
        println!("{:#?}", e);
    }
    // panic!("k");
}

fn expr(l: &mut Lexer, n: usize, ctx: &Ctx) -> Result<Expression, String> {
    // println!("expr({}): {:?}", n, l.peek());
    if n < operator_strength("prefix") {
        return seq(l, n, ctx);
    }
    if n == operator_strength("prefix") {
        return pref(l, ctx);
    }
    if n == operator_strength("suffix") {
        return base(l, ctx);
    }
    panic!("unexpected n: {}", n)
}

fn parens(l: &mut Lexer, ctx: &Ctx) -> Result<Expression, String> {
    expect(l, "(", Some("parenthesized expression"))?;
    let e = expr(l, 0, ctx);
    expect(l, ")", Some("parenthesized expression"))?;
    return e;
}

fn seq(l: &mut Lexer, n: usize, ctx: &Ctx) -> Result<Expression, String> {
    let mut r = expr(l, n + 1, ctx)?;
    while op_follows(l, n) {
        let op = l.get().unwrap().kind;
        let r2 = expr(l, n + 1, ctx)?;
        r = Expression::BinaryOp {
            op,
            a: Box::new(r),
            b: Box::new(r2),
        }
    }
    return Ok(r);
}

fn op_follows(l: &mut Lexer, strength: usize) -> bool {
    if !l.more() {
        return false;
    }
    let next = &l.peek().unwrap().kind;
    return is_op(&next) && operator_strength(&next) == strength;
}

fn pref(l: &mut Lexer, ctx: &Ctx) -> Result<Expression, String> {
    // println!("  pref(): {:?}", l.peek());
    let got = l.get().unwrap();
    let next = String::from(&got.kind);

    if is_prefix_op(&next) {
        let r = pref(l, ctx)?;
        return Ok(Expression::PrefixOperator {
            operator: next,
            operand: Box::new(r),
        });
    }

    // cast
    if next == "("
        && l.peek().unwrap().kind == "word"
        && is_type(l.peek().unwrap().content.as_ref().unwrap(), &ctx.typenames)
    {
        let typeform = parse_anonymous_typeform(l)?;
        expect(l, ")", Some("typecast"))?;
        return Ok(Expression::Cast {
            type_name: typeform,
            operand: Box::new(pref(l, ctx)?),
        });
    }

    if next == "sizeof" {
        l.unget(got);
        return parse_sizeof(l, ctx);
    }

    l.unget(got);
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
            let index = expr(l, 0, ctx)?;
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

fn expr_id(l: &mut Lexer, ctx: &Ctx) -> Result<Expression, String> {
    if !l.more() {
        return Err(String::from("id: unexpected end of input"));
    }
    let next = l.get().unwrap();

    // mod "." word ?
    if next.kind == "word"
        && ctx.modnames.contains(&next.content.as_ref().unwrap())
        && l.peek().unwrap().kind == "."
        && l.peek_n(1).unwrap().kind == "word"
    {
        expect(l, ".", Some("namespaced identifier"))?;
        let t = expect(l, "word", Some("namespaced identifier"))?;
        return Ok(Expression::Identifier(format!(
            "{}_{}",
            next.content.unwrap(),
            t.content.unwrap()
        )));
    }

    if next.kind == "word" {
        return Ok(Expression::Identifier(next.content.unwrap()));
    }
    if next.kind == "num" || next.kind == "string" || next.kind == "char" {
        l.unget(next);
        return Ok(Expression::Literal(parse_literal(l)?));
    }
    if next.kind == "(" {
        l.unget(next);
        return parens(l, ctx);
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
            format!("expected '{}', got '{}' at {}", kind, next.kind, next.pos),
        ));
    }
    return Ok(lexer.get().unwrap());
}

fn parse_identifier(lexer: &mut Lexer) -> Result<String, String> {
    let identifier = String::from(expect(lexer, "word", None)?.content.unwrap());
    return Ok(identifier);
}

fn parse_typename(l: &mut Lexer, comment: Option<&str>) -> Result<Typename, String> {
    let is_const = l.eat("const");
    let name = if l.eat("struct") {
        let name = expect(l, "word", comment)?.content.unwrap();
        format!("struct {}", name)
    } else {
        expect(l, "word", comment)?.content.unwrap()
    };
    return Ok(Typename { is_const, name });
}

fn parse_anonymous_typeform(lexer: &mut Lexer) -> Result<AnonymousTypeform, String> {
    let type_name = parse_typename(lexer, None).unwrap();
    let mut ops: Vec<String> = Vec::new();
    while lexer.follows("*") {
        ops.push(lexer.get().unwrap().kind);
    }
    return Ok(AnonymousTypeform { type_name, ops });
}

fn parse_anonymous_parameters(lexer: &mut Lexer) -> Result<AnonymousParameters, String> {
    let mut params = AnonymousParameters {
        ellipsis: false,
        forms: Vec::new(),
    };
    expect(lexer, "(", Some("anonymous function parameters")).unwrap();
    if !lexer.follows(")") {
        params.forms.push(parse_anonymous_typeform(lexer).unwrap());
        while lexer.follows(",") {
            lexer.get();
            if lexer.follows("...") {
                lexer.get();
                params.ellipsis = true;
                break;
            }
            params.forms.push(parse_anonymous_typeform(lexer).unwrap());
        }
    }
    expect(lexer, ")", Some("anonymous function parameters"))?;
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
        let id = parse_identifier(lexer)?;
        let value: Option<Expression> = if lexer.eat("=") {
            Some(expr(lexer, 0, ctx)?)
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
    return Ok(ModuleObject::Enum(Enum { is_pub, members }));
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
        let key = Expression::Identifier(parse_identifier(l)?);
        expect(l, "=", Some("struct literal member"))?;
        let value = expr(l, 0, ctx)?;
        return Ok(CompositeLiteralEntry {
            is_index: false,
            key: Some(key),
            value,
        });
    }
    if l.follows("[") {
        l.get();
        let key = expr(l, 0, ctx)?;
        expect(l, "]", Some("array literal index"))?;
        expect(l, "=", Some("array literal index"))?;
        let value = expr(l, 0, ctx)?;
        return Ok(CompositeLiteralEntry {
            is_index: true,
            key: Some(key),
            value,
        });
    }
    return Ok(CompositeLiteralEntry {
        is_index: false,
        key: None,
        value: expr(l, 0, ctx)?,
    });
}

fn parse_sizeof(lexer: &mut Lexer, ctx: &Ctx) -> Result<Expression, String> {
    expect(lexer, "sizeof", None)?;
    expect(lexer, "(", None)?;
    let argument: SizeofArgument;

    if lexer.peek().unwrap().kind == "word"
        && is_type(
            lexer.peek().unwrap().content.as_ref().unwrap(),
            &ctx.typenames,
        )
    {
        argument = SizeofArgument::Typename(parse_typename(lexer, None)?);
    } else {
        argument = SizeofArgument::Expression(expr(lexer, 0, ctx).unwrap());
    }
    expect(lexer, ")", None)?;
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
        arguments.push(expr(l, 0, ctx)?);
        while l.follows(",") {
            l.get();
            arguments.push(expr(l, 0, ctx)?);
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
    let condition = expr(lexer, 0, ctx)?;
    expect(lexer, ")", None)?;
    let body = parse_body(lexer, ctx)?;
    return Ok(Statement::While { condition, body });
}

fn parse_body(lexer: &mut Lexer, ctx: &Ctx) -> Result<Body, String> {
    let mut statements: Vec<Statement> = Vec::new();
    if lexer.follows("{") {
        expect(lexer, "{", None)?;
        while !lexer.follows("}") {
            statements.push(parse_statement(lexer, ctx)?);
        }
        expect(lexer, "}", None)?;
    } else {
        statements.push(parse_statement(lexer, ctx)?);
    }
    return Ok(Body { statements });
}

fn parse_statement(lexer: &mut Lexer, ctx: &Ctx) -> Result<Statement, String> {
    let next = lexer.peek().unwrap();
    if (next.kind == "word" && is_type(&next.content.as_ref().unwrap(), &ctx.typenames))
        || next.kind == "const"
    {
        return parse_variable_declaration(lexer, ctx);
    }
    return match next.kind.as_str() {
        "if" => parse_if(lexer, ctx),
        "for" => parse_for(lexer, ctx),
        "while" => parse_while(lexer, ctx),
        "return" => parse_return(lexer, ctx),
        "switch" => parse_switch(lexer, ctx),
        _ => {
            let expr = expr(lexer, 0, ctx)?;
            expect(lexer, ";", Some("parsing statement"))?;
            return Ok(Statement::Expression(expr));
        }
    };
}

fn parse_variable_declaration(lexer: &mut Lexer, ctx: &Ctx) -> Result<Statement, String> {
    let type_name = parse_typename(lexer, None)?;
    let mut forms = vec![];
    let mut values = vec![];
    loop {
        forms.push(parse_form(lexer, ctx)?);
        if lexer.eat("=") {
            values.push(Some(expr(lexer, 0, ctx)?));
        } else {
            values.push(None)
        };
        if !lexer.eat(",") {
            break;
        }
    }
    expect(lexer, ";", None)?;
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
    let expression = expr(lexer, 0, ctx)?;
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
        &lexer.get().unwrap();
        let e: Option<Expression>;
        if lexer.more() && lexer.peek().unwrap().kind != "]" {
            e = Some(expr(lexer, 0, ctx)?);
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
    condition = expr(lexer, 0, ctx)?;
    expect(lexer, ")", Some("if statement"))?;
    body = parse_body(lexer, ctx)?;
    if lexer.follows("else") {
        lexer.get();
        else_body = Some(parse_body(lexer, ctx)?);
    }
    return Ok(Statement::If {
        condition,
        body,
        else_body,
    });
}

fn parse_for(lexer: &mut Lexer, ctx: &Ctx) -> Result<Statement, String> {
    expect(lexer, "for", None)?;
    expect(lexer, "(", None)?;

    let init: ForInit;
    if lexer.peek().unwrap().kind == "word"
        && is_type(
            &lexer.peek().unwrap().content.as_ref().unwrap(),
            &ctx.typenames,
        )
    {
        let type_name = parse_typename(lexer, None)?;
        let form = parse_form(lexer, ctx)?;
        expect(lexer, "=", None)?;
        let value = expr(lexer, 0, ctx)?;
        init = ForInit::LoopCounterDeclaration {
            type_name,
            form,
            value,
        };
    } else {
        init = ForInit::Expression(expr(lexer, 0, ctx)?);
    }

    expect(lexer, ";", None)?;
    let condition = expr(lexer, 0, ctx)?;
    expect(lexer, ";", None)?;
    let action = expr(lexer, 0, ctx)?;
    expect(lexer, ")", None)?;
    let body = parse_body(lexer, ctx)?;

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
    let value = expr(lexer, 0, ctx)?;
    let mut cases: Vec<SwitchCase> = vec![];
    expect(lexer, ")", None)?;
    expect(lexer, "{", None)?;
    while lexer.follows("case") {
        expect(lexer, "case", None)?;
        let case_value = if lexer.follows("word") {
            SwitchCaseValue::Identifier(parse_identifier(lexer)?)
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
    let body = parse_body(lexer, ctx)?;
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
    let type_name = parse_typename(lexer, None)?;
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
        let type_name = parse_typename(lexer, None)?;
        let form = parse_form(lexer, ctx)?;
        expect(lexer, ";", None)?;
        fields.push(UnionField { type_name, form });
    }
    expect(lexer, "}", None)?;
    let form = parse_form(lexer, ctx)?;
    expect(lexer, ";", None)?;
    return Ok(Union { form, fields });
}

fn parse_module_object(lexer: &mut Lexer, ctx: &Ctx) -> Result<ModuleObject, String> {
    let mut is_pub = false;
    if lexer.follows("pub") {
        lexer.get();
        is_pub = true;
    }
    if lexer.follows("enum") {
        return parse_enum(lexer, is_pub, ctx);
    }
    if lexer.follows("typedef") {
        return parse_typedef(is_pub, lexer, ctx);
    }
    let type_name = parse_typename(lexer, None)?;
    let form = parse_form(lexer, ctx)?;
    if lexer.peek().unwrap().kind == "(" {
        return parse_function_declaration(lexer, is_pub, type_name, form, ctx);
    }

    if lexer.peek().unwrap().kind != "=" {
        return Err("module variable: '=' expected".to_string());
    }

    if lexer.peek().unwrap().kind == "=" {
        if is_pub {
            return Err("module variables can't be exported".to_string());
        }
        lexer.get();
        let value = expr(lexer, 0, ctx)?;
        expect(lexer, ";", Some("module variable declaration"))?;
        let var = ModuleVariable {
            type_name,
            form,
            value,
        };
        return Ok(ModuleObject::ModuleVariable(var));
    }
    return Err("unexpected input".to_string());
}

fn parse_compat_macro(lexer: &mut Lexer) -> Result<CompatMacro, String> {
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

    return Ok(CompatMacro {
        name: name[1..].to_string(),
        value: value.to_string(),
    });
}

fn parse_typedef(is_pub: bool, lexer: &mut Lexer, ctx: &Ctx) -> Result<ModuleObject, String> {
    expect(lexer, "typedef", None)?;

    if lexer.follows("{") {
        // typedef {int hour, minute, second} time;

        // Parse the struct
        let mut fields: Vec<StructEntry> = vec![];
        expect(lexer, "{", Some("struct type definition"))?;
        while lexer.more() && lexer.peek().unwrap().kind != "}" {
            if lexer.peek().unwrap().kind == "union" {
                fields.push(StructEntry::Union(parse_union(lexer, ctx)?));
            } else {
                fields.push(StructEntry::Plain(parse_type_and_forms(lexer, ctx)?));
            }
        }
        expect(lexer, "}", None)?;

        // Parse the name
        let name = parse_identifier(lexer)?;
        expect(lexer, ";", Some("typedef"))?;
        return Ok(ModuleObject::StructTypedef(StructTypedef {
            is_pub,
            fields,
            name,
        }));
    }

    // typedef void *f(int a)[20];
    // typedef foo bar;
    // typedef struct foo foo_t;
    let type_name = parse_typename(lexer, Some("typedef"))?;

    if lexer.follows("(") {
        // Function pointer type:
        // typedef int (*func_t)(FILE*, const char *, ...);
        lexer.get();
        expect(lexer, "*", Some("function pointer typedef"))?;
        let name = parse_identifier(lexer)?;
        expect(lexer, ")", Some("function pointer typedef"))?;
        let params = parse_anonymous_parameters(lexer)?;
        expect(lexer, ";", Some("typedef"))?;
        return Ok(ModuleObject::FuncTypedef(FuncTypedef {
            is_pub,
            return_type: type_name,
            name,
            params,
        }));
    }

    let mut stars = String::new();
    while lexer.follows("*") {
        stars += &lexer.get().unwrap().kind;
    }
    let alias = parse_identifier(lexer)?;

    let mut params: Option<AnonymousParameters> = None;
    if lexer.follows("(") {
        params = Some(parse_anonymous_parameters(lexer)?);
    }
    let mut size: usize = 0;
    if lexer.follows("[") {
        lexer.get();
        size = expect(lexer, "num", None)
            .unwrap()
            .content
            .unwrap()
            .parse()
            .unwrap();
        expect(lexer, "]", None)?;
    }

    let form = TypedefForm {
        stars,
        params,
        size,
        alias,
    };

    expect(lexer, ";", Some("typedef"))?;
    return Ok(ModuleObject::Typedef(Typedef {
        is_pub,
        type_name,
        form,
    }));
}

fn parse_type_and_forms(lexer: &mut Lexer, ctx: &Ctx) -> Result<TypeAndForms, String> {
    if lexer.follows("struct") {
        return Err("can't parse nested structs, please consider a typedef".to_string());
    }

    let type_name = parse_typename(lexer, None)?;

    let mut forms: Vec<Form> = vec![];
    forms.push(parse_form(lexer, ctx)?);
    while lexer.follows(",") {
        lexer.get();
        forms.push(parse_form(lexer, ctx)?);
    }

    expect(lexer, ";", None)?;
    return Ok(TypeAndForms { type_name, forms });
}

fn skip_brackets(lexer: &mut Lexer) {
    expect(lexer, "{", None).unwrap();
    while lexer.more() {
        if lexer.follows("{") {
            skip_brackets(lexer);
            continue;
        }
        if lexer.follows("}") {
            break;
        }
        lexer.get();
    }
    expect(lexer, "}", None).unwrap();
}

fn get_typename(lexer: &mut Lexer) -> Result<String, String> {
    // The type name is at the end of the typedef statement.
    // typedef foo bar;
    // typedef {...} bar;
    // typedef struct foo bar;

    if lexer.follows("{") {
        skip_brackets(lexer);
        let name = expect(lexer, "word", None).unwrap().content.unwrap();
        expect(lexer, ";", None)?;
        return Ok(name);
    }

    // Get all tokens until the semicolon.
    let mut buf: Vec<Token> = vec![];
    while !lexer.ended() {
        let t = lexer.get().unwrap();
        if t.kind == ";" {
            break;
        }
        buf.push(t);
    }

    if buf.is_empty() {
        return Err("No tokens after 'typedef'".to_string());
    }

    buf.reverse();

    // We assume that function typedefs end with "(...)".
    // In that case we omit that part.
    if buf[0].kind == ")" {
        while !buf.is_empty() {
            let t = buf.remove(0);
            if t.kind == "(" {
                break;
            }
        }
    }

    let mut name: Option<String> = None;

    // The last "word" token is assumed to be the type name.
    while !buf.is_empty() {
        let t = buf.remove(0);
        if t.kind == "word" {
            name = Some(t.content.unwrap());
            break;
        }
    }

    if name.is_none() {
        return Err("Type name expected in the typedef".to_string());
    }
    return Ok(name.unwrap());
}

fn path_to_ns(path: String) -> String {
    return if path.contains("/") {
        path.split("/").last().unwrap().to_string()
    } else {
        path
    };
}

fn get_file_deps(path: &str) -> Result<Vec<String>, String> {
    let mut lexer = for_file(path)?;
    let mut list: Vec<String> = vec![];
    loop {
        match lexer.get() {
            None => break,
            Some(t) => {
                if t.kind == "import" {
                    list.push(path_to_ns(t.content.unwrap()))
                }
            }
        }
    }
    return Ok(list);
}

fn get_file_typenames(path: &str) -> Result<Vec<String>, String> {
    // Scan file tokens for 'typedef' keywords
    let mut lexer = for_file(path)?;
    let mut list: Vec<String> = vec![];
    loop {
        let t = lexer.get();
        if t.is_none() {
            break;
        }

        let tt = t.unwrap();
        // When a 'typedef' is encountered, look ahead
        // to find the type name
        if tt.kind == "typedef" {
            list.push(get_typename(&mut lexer)?);
        }
        if tt.kind == "macro" && tt.content.as_ref().unwrap().starts_with("#type") {
            let name = tt.content.unwrap()[6..].trim().to_string();
            list.push(name);
        }
    }
    return Ok(list);
}

pub fn homepath() -> String {
    return env::var("CHELANG_HOME").unwrap_or(String::from("."));
}

pub fn get_module(name: &String) -> Result<Module, String> {
    // If requested module name ends with ".c", we look for it at the given
    // location. If ".c" is omitted, we look for it inside the hardcoded lib
    // directory.
    let module_path = if name.ends_with(".c") {
        name.clone()
    } else {
        format!("{}/lib/{}.c", homepath(), name)
    };

    if std::fs::metadata(&module_path).is_err() {
        return Err(format!(
            "can't find module '{}' (looked at {})",
            name, module_path
        ));
    }

    let ctx = Ctx {
        typenames: get_file_typenames(&module_path)?,
        modnames: get_file_deps(&module_path)?,
    };
    let mut lexer = for_file(&module_path)?;
    let elements = parse_module(&mut lexer, &ctx);
    if elements.is_err() {
        let next = lexer.peek().unwrap();
        let wher = format!("{}: {}", module_path, lexer.peek().unwrap().pos);
        let what = elements.err().unwrap();
        return Err(format!("{}: {}: {}...", wher, what, token_to_string(next)));
    }

    let els = elements.unwrap();
    return Ok(Module {
        elements: els,
        id: format!("{}", Path::new(&module_path).display()),
    });
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

fn parse_module(lexer: &mut Lexer, ctx: &Ctx) -> Result<Vec<ModuleObject>, String> {
    let mut elements: Vec<ModuleObject> = vec![];
    let mut ctx2 = Ctx {
        modnames: ctx.modnames.clone(),
        typenames: ctx.typenames.clone(),
    };
    while lexer.more() {
        match lexer.peek().unwrap().kind.as_str() {
            "import" => {
                let tok = lexer.get().unwrap();
                let path = tok.content.unwrap();
                let p = path.clone();
                elements.push(ModuleObject::Import { path });
                let module = get_module(&p)?;
                for element in module.elements {
                    match element {
                        ModuleObject::Typedef(Typedef { form, is_pub, .. }) => {
                            if is_pub {
                                ctx2.typenames.push(form.alias);
                            }
                        }
                        ModuleObject::StructTypedef(StructTypedef { name, is_pub, .. }) => {
                            if is_pub {
                                ctx2.typenames.push(name);
                            }
                        }
                        _ => {}
                    }
                }
            }
            "macro" => {
                elements.push(ModuleObject::CompatMacro(parse_compat_macro(lexer)?));
            }
            _ => {
                elements.push(parse_module_object(lexer, &ctx2)?);
            }
        }
    }
    return Ok(elements);
}
