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
