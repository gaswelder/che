use crate::buf::Buf;
use serde::{Deserialize, Serialize};
use std::fs;

#[derive(Serialize, Deserialize, Debug)]
pub struct Token {
    pub kind: String,
    pub content: Option<String>,
    pub pos: String,
}

const SPACES: &str = "\r\n\t ";

pub fn read_file(filename: &str) -> Result<Vec<Token>, String> {
    let mut toks: Vec<Token> = Vec::new();
    let contents = fs::read_to_string(filename).expect("Something went wrong reading the file");
    let mut b = crate::buf::new(contents);
    loop {
        let r = read_token(&mut b);
        if r.is_none() {
            break;
        }
        let tok = r.unwrap();
        if tok.kind == "error" {
            let err = format!("{} at {}", tok.content.unwrap(), tok.pos);
            return Err(err);
        }
        toks.push(tok)
    }
    return Ok(toks);
}

pub fn read_token(buf: &mut Buf) -> Option<Token> {
    buf.read_set(SPACES.to_string());

    if buf.ended() {
        return None;
    }

    let pos = buf.pos();

    if buf.peek().unwrap() == '#' {
        return Some(Token {
            kind: "macro".to_string(),
            content: Some(buf.skip_until('\n')),
            pos,
        });
    }

    if buf.literal_follows("/*") {
        return Some(read_multiline_comment(buf));
    }

    if buf.skip_literal("//") {
        return Some(Token {
            kind: "comment".to_string(),
            content: Some(buf.skip_until('\n')),
            pos,
        });
    }

    let next = buf.peek().unwrap();
    if next.is_ascii_alphabetic() || next == '_' {
        return Some(read_word(buf));
    }

    if next.is_ascii_digit() {
        return Some(read_number(buf));
    }

    if next == '"' {
        return Some(read_string_literal(buf));
    }

    if next == '\'' {
        return Some(read_char_literal(buf));
    }

    // Sorted by length, longest first.
    let symbols = [
        "<<=", ">>=", "...", "++", "--", "->", "<<", ">>", "<=", ">=", "&&", "||", "+=", "-=",
        "*=", "/=", "%=", "&=", "^=", "|=", "==", "!=", "!", "~", "&", "^", "*", "/", "%", "=",
        "|", ":", ",", "<", ">", "+", "-", "{", "}", ";", "[", "]", "(", ")", ".", "?",
    ];
    for sym in &symbols {
        if buf.skip_literal(sym) {
            return Some(Token {
                kind: sym.to_string(),
                content: None,
                pos,
            });
        }
    }

    return Some(Token {
        kind: "error".to_string(),
        content: Some(format!("Unexpected character: '{}'", next).to_string()),
        pos,
    });
}

fn read_number(buf: &mut Buf) -> Token {
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

fn read_hex(buf: &mut Buf) -> Token {
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

fn read_string_literal(buf: &mut Buf) -> Token {
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

fn read_word(buf: &mut Buf) -> Token {
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

fn read_char_literal(buf: &mut Buf) -> Token {
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

fn read_multiline_comment(buf: &mut Buf) -> Token {
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

#[cfg(test)]
mod tests {
    use super::*;

    fn _read_token(s: &str) -> Token {
        let mut b = crate::buf::new(s.to_string());
        return read_token(&mut b).unwrap();
    }

    #[test]
    fn read_token_test() {
        struct C<'a> {
            input: &'a str,
            kind: &'a str,
            content: &'a str,
            pos: &'a str,
        }
        let cases = [
            C {
                input: "#type abc\n",
                kind: "macro",
                content: "#type abc",
                pos: "1:1",
            },
            C {
                input: "/* \ncomment /* comment \n*/ 123",
                kind: "comment",
                content: " \ncomment /* comment \n",
                pos: "1:1",
            },
            C {
                input: "// comment\n123",
                kind: "comment",
                content: " comment",
                pos: "1:1",
            },
            C {
                input: "abc_123++",
                kind: "word",
                content: "abc_123",
                pos: "1:1",
            },
            C {
                input: "'a'123",
                kind: "char",
                content: "a",
                pos: "1:1",
            },
            C {
                input: "'\\t'123",
                kind: "char",
                content: "\\t",
                pos: "1:1",
            },
            C {
                input: "0x123 abc",
                kind: "num",
                content: "0x123",
                pos: "1:1",
            },
            C {
                input: "123 abc",
                kind: "num",
                content: "123",
                pos: "1:1",
            },
            C {
                input: "123UL abc",
                kind: "num",
                content: "123UL",
                pos: "1:1",
            },
            C {
                input: "123.45 abc",
                kind: "num",
                content: "123.45",
                pos: "1:1",
            },
            C {
                input: "0x123",
                kind: "num",
                content: "0x123",
                pos: "1:1",
            },
            C {
                input: "0x123UL",
                kind: "num",
                content: "0x123UL",
                pos: "1:1",
            },
            C {
                input: "\"abc\" 123",
                kind: "string",
                content: "abc",
                pos: "1:1",
            },
            C {
                input: "\"ab\\\"c\" 123",
                kind: "string",
                content: "ab\\\"c",
                pos: "1:1",
            },
            C {
                input: "\"abc\" \"def\" 123",
                kind: "string",
                content: "abcdef",
                pos: "1:1",
            },
        ];

        for case in &cases {
            let t = _read_token(case.input);
            assert_eq!(t.content.unwrap(), case.content);
            assert_eq!(t.kind, case.kind);
            assert_eq!(t.pos, case.pos);
        }

        let symbols = [
            "<<=", ">>=", "...", "++", "--", "->", "<<", ">>", "<=", ">=", "&&", "||", "+=", "-=",
            "*=", "/=", "%=", "&=", "^=", "|=", "==", "!=", "!", "~", "&", "^", "*", "/", "%", "=",
            "|", ":", ",", "<", ">", "+", "-", "{", "}", ";", "[", "]", "(", ")", ".", "?",
        ];
        for sym in &symbols {
            let t = _read_token(format!("{}123", sym).as_str());
            assert_eq!(t.content.is_none(), true);
            assert_eq!(t.kind, sym.to_string());
            assert_eq!(t.pos, "1:1");
        }

        let keywords = [
            "default", "typedef", "struct", "import", "union", "const", "return", "switch",
            "sizeof", "while", "defer", "case", "enum", "else", "for", "pub", "if",
        ];
        for kw in &keywords {
            let t = _read_token(format!("{} 123", kw).as_str());
            assert_eq!(t.content.is_none(), true);
            assert_eq!(t.kind, kw.to_string());
            assert_eq!(t.pos, "1:1");
        }
    }
}
