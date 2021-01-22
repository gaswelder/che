use crate::buf::Buf;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize)]
pub struct Token {
    pub kind: String,
    pub content: Option<String>,
    pub pos: String,
}

const SPACES: &str = "\r\n\t ";

pub fn read_number(buf: &mut Buf) -> Token {
    if buf.literal_follows("0x") {
        return read_hex(buf);
    }

    let pos = buf.pos();
    let alpha = "0123456789";
    let mut num = buf.read_set(alpha.to_string());

    let mut modifiers = String::from("UL");

    if buf.more() && buf.peek().unwrap() == '.' {
        modifiers += "f";
        num += &buf.get().unwrap().to_string();
        num += &buf.read_set(alpha.to_string());
    }

    num += &buf.read_set(modifiers);

    if buf.more() && buf.peek().unwrap().is_ascii_alphabetic() {
        let c = buf.peek().unwrap();
        return Token {
            kind: "error".to_string(),
            content: Some(format!("Unexpected character: '{}'", c)),
            pos: buf.pos(),
        };
    }
    return Token {
        kind: "num".to_string(),
        content: Some(num),
        pos,
    };
}

pub fn read_hex(buf: &mut Buf) -> Token {
    let pos = buf.pos();

    // Skip "0x"
    buf.get();
    buf.get();

    let num = buf.read_set("0123456789ABCDEFabcdef".to_string()) + &buf.read_set("UL".to_string());

    return Token {
        kind: "num".to_string(),
        content: Some(String::from("0x") + &num),
        pos,
    };
}

pub fn read_string_literal(buf: &mut Buf) -> Token {
    let pos = buf.pos();
    let mut s = String::new();
    // A string literal may be split into parts.
    while buf.more() && buf.peek().unwrap() == '"' {
        buf.get();
        let mut substr = String::new();
        while buf.more() && buf.peek().unwrap() != '"' {
            let ch = buf.get().unwrap();
            substr += &ch.to_string();
            if ch == '\\' {
                substr += &buf.get().unwrap().to_string();
            }
        }
        if !buf.more() || buf.get().unwrap() != '"' {
            return Token {
                kind: "error".to_string(),
                content: Some("Double quote expected".to_string()),
                pos,
            };
        }
        s += &substr;
        buf.read_set(SPACES.to_string());
    }
    return Token {
        kind: "string".to_string(),
        content: Some(s),
        pos,
    };
}

pub fn read_word(buf: &mut Buf) -> Token {
    let pos = buf.pos();
    let mut word = String::new();
    while buf.more() {
        let ch = buf.peek().unwrap();
        if !ch.is_alphanumeric() && ch != '_' {
            break;
        }
        word.push(buf.get().unwrap());
    }

    let keywords = [
        "default", "typedef", "struct", "import", "union", "const", "return", "switch", "sizeof",
        "while", "defer", "case", "enum", "else", "for", "pub", "if",
    ];

    if keywords.contains(&word.as_str()) {
        return Token {
            kind: word,
            content: None,
            pos,
        };
    }
    return Token {
        kind: "word".to_string(),
        content: Some(word),
        pos,
    };
}

pub fn read_char_literal(buf: &mut Buf) -> Token {
    let pos = buf.pos();
    buf.get();

    let mut s = String::new();
    if buf.more() && buf.peek().unwrap() == '\\' {
        s.push(buf.get().unwrap());
    }

    s.push(buf.get().unwrap());
    if buf.get().unwrap() != '\'' {
        return Token {
            kind: "error".to_string(),
            content: Some("Single quote expected".to_string()),
            pos,
        };
    }
    return Token {
        kind: "char".to_string(),
        content: Some(s),
        pos,
    };
}

pub fn read_multiline_comment(buf: &mut Buf) -> Token {
    let pos = buf.pos();
    buf.skip_literal("/*");
    let comment = buf.until_literal("*/");
    if !buf.skip_literal("*/") {
        return Token {
            kind: "error".to_string(),
            content: Some("*/ expected".to_string()),
            pos,
        };
    }
    return Token {
        kind: "comment".to_string(),
        content: Some(comment),
        pos,
    };
}
