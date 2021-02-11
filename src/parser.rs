use crate::lexer::{Lexer, Token};
use serde::{Deserialize, Serialize, Serializer};

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

#[derive(Serialize, Deserialize, Debug)]
pub struct Enum {
    kind: String,
    is_pub: bool,
    members: Vec<EnumMember>,
}
#[derive(Serialize, Deserialize, Debug)]
pub struct EnumMember {
    kind: String,
    id: Identifier,
    value: Option<Literal>,
}
pub fn parse_enum(lexer: &mut Lexer, is_pub: bool) -> Result<Enum, String> {
    let mut members: Vec<EnumMember> = Vec::new();
    expect(lexer, "enum", Some("enum definition")).unwrap();
    expect(lexer, "{", Some("enum definition")).unwrap();
    loop {
        let id = parse_identifier(lexer);
        let mut value: Option<Literal> = None;
        if lexer.follows("=") {
            lexer.get();
            value = Some(parse_literal(lexer)?);
        }
        members.push(EnumMember {
            kind: "c_enum_member".to_string(),
            id: id.unwrap(),
            value,
        });
        if lexer.follows(",") {
            lexer.get();
            continue;
        } else {
            break;
        }
    }
    expect(lexer, "}", Some("enum definition")).unwrap();
    expect(lexer, ";", Some("enum definition")).unwrap();
    return Ok(Enum {
        kind: "c_enum".to_string(),
        is_pub,
        members,
    });
}

#[derive(Debug, Serialize)]
pub struct ArrayLiteral {
    kind: String,
    values: Vec<ArrayLiteralEntry>,
}

pub fn parse_array_literal(lexer: &mut Lexer) -> Result<ArrayLiteral, String> {
    let mut values: Vec<ArrayLiteralEntry> = Vec::new();
    expect(lexer, "{", Some("array literal"))?;
    if !lexer.follows("}") {
        values.push(parse_array_literal_entry(lexer).unwrap());
        while lexer.follows(",") {
            lexer.get();
            values.push(parse_array_literal_entry(lexer).unwrap());
        }
    }
    expect(lexer, "}", Some("array literal"))?;
    return Ok(ArrayLiteral {
        kind: "c_array_literal".to_string(),
        values,
    });
}

#[derive(Debug)]
enum ArrayLiteralKey {
    None,
    Identifier(Identifier),
    Literal(Literal),
}

impl Serialize for ArrayLiteralKey {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            ArrayLiteralKey::None => serializer.serialize_none(),
            ArrayLiteralKey::Identifier(x) => Serialize::serialize(x, serializer),
            ArrayLiteralKey::Literal(x) => Serialize::serialize(x, serializer),
        }
    }
}

#[derive(Debug)]
enum ArrayLiteralValue {
    ArrayLiteral(ArrayLiteral),
    Identifier(Identifier),
    Literal(Literal),
}

impl Serialize for ArrayLiteralValue {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            ArrayLiteralValue::ArrayLiteral(x) => Serialize::serialize(x, serializer),
            ArrayLiteralValue::Identifier(x) => Serialize::serialize(x, serializer),
            ArrayLiteralValue::Literal(x) => Serialize::serialize(x, serializer),
        }
    }
}

#[derive(Debug, Serialize)]
pub struct ArrayLiteralEntry {
    index: ArrayLiteralKey,
    value: ArrayLiteralValue,
}

pub fn parse_array_literal_entry(lexer: &mut Lexer) -> Result<ArrayLiteralEntry, String> {
    let mut index: ArrayLiteralKey = ArrayLiteralKey::None;
    if lexer.follows("[") {
        lexer.get();
        if lexer.follows("word") {
            index = ArrayLiteralKey::Identifier(parse_identifier(lexer)?);
        } else {
            index = ArrayLiteralKey::Literal(parse_literal(lexer)?);
        }
        expect(lexer, "]", Some("array literal entry"))?;
        expect(lexer, "=", Some("array literal entry"))?;
    }
    let value: ArrayLiteralValue;
    if lexer.follows("{") {
        value = ArrayLiteralValue::ArrayLiteral(parse_array_literal(lexer)?);
    } else if lexer.follows("word") {
        value = ArrayLiteralValue::Identifier(parse_identifier(lexer)?);
    } else {
        value = ArrayLiteralValue::Literal(parse_literal(lexer)?);
    }

    return Ok(ArrayLiteralEntry { index, value });
}
