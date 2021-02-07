use crate::lexer::{Lexer, Token};
use serde::{Deserialize, Serialize};

pub fn is_op(token_type: String) -> bool {
    let ops = [
        "+", "-", "*", "/", "=", "|", "&", "~", "^", "<", ">", "?", ":", "%", "+=", "-=", "*=",
        "/=", "%=", "&=", "^=", "|=", "++", "--", "->", ".", ">>", "<<", "<=", ">=", "&&", "||",
        "==", "!=", "<<=", ">>=",
    ];
    for op in ops.iter() {
        if token_type.eq(op) {
            return true;
        }
    }
    return false;
}

pub fn operator_strength(op: String) -> usize {
    let map = [
        vec![","],
        vec![
            "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=",
        ],
        vec!["||"],
        vec!["&&"],
        vec!["|"],
        vec!["^"],
        vec!["&"],
        vec!["!=", "=="],
        vec![">", "<", ">=", "<="],
        vec!["<<", ">>"],
        vec!["+", "-"],
        vec!["*", "/", "%"],
        vec!["prefix"],
        vec!["->", "."],
    ];
    for (i, ops) in map.iter().enumerate() {
        if ops.contains(&op.as_str()) {
            return i + 1;
        }
    }
    panic!("unknown operator: '{}'", op);
}

pub fn is_postfix_op(token: String) -> bool {
    let ops = ["++", "--", "(", "["];
    return ops.to_vec().contains(&token.as_str());
}

pub fn is_prefix_op(op: String) -> bool {
    let ops = ["!", "--", "++", "*", "~", "&", "-"];
    return ops.to_vec().contains(&op.as_str());
}

pub fn is_type(name: String, typenames: &Vec<String>) -> bool {
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
    return types.to_vec().contains(&name.as_str())
        || typenames.to_vec().contains(&name)
        || name.ends_with("_t");
}

fn with_comment(comment: Option<&str>, message: String) -> String {
    match comment {
        Some(s) => format!("{}: {}", s, message),
        None => message.to_string(),
    }
}

pub fn expect(lexer: &mut Lexer, kind: &str, comment: Option<&str>) -> Result<Token, String> {
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
            format!("expected '{}', got {} at {}", kind, next.kind, next.pos),
        ));
    }
    return Ok(lexer.get().unwrap());
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Identifier {
    kind: String,
    name: String,
}

pub fn parse_identifier(lexer: &mut Lexer) -> Result<Identifier, String> {
    let tok = expect(lexer, "word", None)?;
    let name = tok.content;
    return Ok(Identifier {
        kind: "c_identifier".to_string(),
        name: name.unwrap(),
    });
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Type {
    kind: String,
    is_const: bool,
    type_name: String,
}

pub fn parse_type(lexer: &mut Lexer, comment: Option<&str>) -> Result<Type, String> {
    let mut is_const = false;
    let type_name: String;

    if lexer.follows("const") {
        lexer.get();
        is_const = true;
    }

    if lexer.follows("struct") {
        lexer.get();
        let name = expect(lexer, "word", comment)?.content.unwrap();
        type_name = format!("struct {}", name);
    } else {
        type_name = expect(lexer, "word", comment)?.content.unwrap();
    }

    return Ok(Type {
        kind: "c_type".to_string(),
        is_const,
        type_name,
    });
}

#[derive(Serialize, Deserialize, Debug)]
pub struct AnonymousTypeform {
    kind: String,
    type_name: Type,
    ops: Vec<String>,
}

pub fn parse_anonymous_typeform(lexer: &mut Lexer) -> Result<AnonymousTypeform, String> {
    let type_name = parse_type(lexer, None).unwrap();
    let mut ops: Vec<String> = Vec::new();
    while lexer.follows("*") {
        ops.push(lexer.get().unwrap().kind);
    }
    return Ok(AnonymousTypeform {
        kind: "c_anonymous_typeform".to_string(),
        type_name: type_name,
        ops,
    });
}

#[derive(Serialize, Deserialize, Debug)]
pub struct AnonymousParameters {
    kind: String,
    forms: Vec<AnonymousTypeform>,
}
pub fn parse_anonymous_parameters(lexer: &mut Lexer) -> Result<AnonymousParameters, String> {
    let mut forms: Vec<AnonymousTypeform> = Vec::new();
    expect(lexer, "(", Some("anonymous function parameters")).unwrap();
    if !lexer.follows(")") {
        forms.push(parse_anonymous_typeform(lexer).unwrap());
        while lexer.follows(",") {
            lexer.get();
            forms.push(parse_anonymous_typeform(lexer).unwrap());
        }
    }
    expect(lexer, ")", Some("anonymous function parameters")).unwrap();
    return Ok(AnonymousParameters {
        kind: "c_anonymous_parameters".to_string(),
        forms,
    });
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Literal {
    kind: String,
    type_name: String,
    value: String,
}
pub fn parse_literal(lexer: &mut Lexer) -> Result<Literal, String> {
    let types = ["string", "num", "char"];
    for t in types.iter() {
        if lexer.peek().unwrap().kind != t.to_string() {
            continue;
        }
        let value = lexer.get().unwrap().content.unwrap();
        return Ok(Literal {
            kind: "c_literal".to_string(),
            type_name: t.to_string(),
            value: value,
        });
    }
    let next = lexer.peek().unwrap();
    if next.kind == "word" && next.content.as_ref().unwrap() == "NULL" {
        lexer.get();
        return Ok(Literal {
            kind: "c_literal".to_string(),
            type_name: "null".to_string(),
            value: "NULL".to_string(),
        });
    }
    return Err(format!("literal expected, got {}", lexer.peek().unwrap()));
}
