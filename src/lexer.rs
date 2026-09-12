use crate::buf::Buf;
use crate::buf::Pos;
use std::collections::VecDeque;
use std::fmt;
use std::fs;
use substring::Substring;

#[derive(Debug)]
pub struct Token {
    pub comments: Option<Vec<String>>,
    pub trailing_comment: Option<String>,
    pub kind: String,
    pub content: String,
    pub pos: Pos,
}

impl fmt::Display for Token {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let c = self.content.substring(0, 40);
        return write!(f, "[{} {}]", self.kind, c);
    }
}

const SPACES: &str = "\r\n\t ";
const DIGITS: &str = "0123456789";

const KEYWORDS: &[&str] = &[
    "break", "case", "const", "continue", "default", "else", "enum", "for", "if", "pub", "return",
    "sizeof", "struct", "switch", "typedef", "union", "while",
];

// Sorted by length, longest first.
const SYMBOLS: &[&str] = &[
    "<<=", ">>=", "...", "++", "--", "->", "<<", ">>", "<=", ">=", "&&", "||", "+=", "-=", "*=",
    "/=", "%=", "&=", "^=", "|=", "==", "!=", "!", "~", "&", "^", "*", "/", "%", "=", "|", ":",
    ",", "<", ">", "+", "-", "{", "}", ";", "[", "]", "(", ")", ".", "?",
];

fn read_token_c(buf: &mut Buf) -> Option<Token> {
    let mut comments = Vec::new();
    loop {
        match read_token(buf) {
            Some(mut tok) => {
                if tok.kind == "comment" {
                    comments.push(tok.content);
                    continue;
                }
                if comments.len() > 0 {
                    tok.comments = Some(comments);
                }
                return Some(tok);
            }
            None => {
                return None;
            }
        }
    }
}

fn read_token(buf: &mut Buf) -> Option<Token> {
    buf.read_set(SPACES);
    if buf.ended() {
        return None;
    }

    let pos = buf.pos();

    if buf.skip_literal("#import") {
        buf.read_set(SPACES);
        return Some(newtok(
            pos,
            "import",
            buf.skip_until('\n').trim().to_string(),
        ));
    }

    if buf.peek().unwrap() == '#' {
        return Some(newtok(pos, "macro", buf.skip_until('\n')));
    }
    if buf.literal_follows("/*") {
        return Some(read_multiline_comment(buf));
    }
    if buf.skip_literal("//") {
        buf.read_set(" \t");
        return Some(newtok(pos, "comment", buf.skip_until('\n')));
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

    for sym in SYMBOLS {
        if buf.skip_literal(sym) {
            let mut tok = newtok(pos, sym, String::new());
            buf.read_set(" \t");
            if buf.skip_literal("//") {
                buf.read_set(" \t");
                tok.trailing_comment = Some(buf.until_literal("\n"));
            }
            return Some(tok);
        }
    }

    return Some(errtok(pos, format!("Unexpected character: '{}'", next)));
}

fn errtok(pos: Pos, msg: String) -> Token {
    newtok(pos, "error", msg)
}

fn newtok(pos: Pos, kind: &str, content: String) -> Token {
    Token {
        comments: None,
        trailing_comment: None,
        kind: kind.to_string(),
        content,
        pos,
    }
}

fn read_number(buf: &mut Buf) -> Token {
    if buf.literal_follows("0x") {
        return read_hex(buf);
    }

    let pos = buf.pos();
    let mut modifiers = String::from("UL");
    let mut num = buf.read_set(DIGITS);

    if buf.more() && buf.peek().unwrap() == '.' {
        modifiers += "f";
        num += &buf.get().unwrap().to_string();
        num += &buf.read_set(DIGITS);
    }

    if buf.more() && (buf.peek().unwrap() == 'e' || buf.peek().unwrap() == 'E') {
        num += &buf.get().unwrap().to_string();
        if buf.peek().unwrap() == '-' {
            num += &buf.get().unwrap().to_string();
        }
        num += &buf.read_set(DIGITS);
    }

    num += &buf.read_set(modifiers.as_str());

    if buf.more() && buf.peek().unwrap().is_ascii_alphabetic() {
        let c = buf.peek().unwrap();
        return errtok(buf.pos(), format!("Unexpected character: '{}'", c));
    }
    return newtok(pos, "num", num);
}

fn read_hex(buf: &mut Buf) -> Token {
    let pos = buf.pos();

    // Skip "0x"
    buf.get();
    buf.get();

    let num = buf.read_set("0123456789ABCDEFabcdef") + &buf.read_set("UL");

    return newtok(pos, "num", format!("0x{}", &num));
}

fn read_string_literal(buf: &mut Buf) -> Token {
    let pos = buf.pos();
    let mut s = String::new();
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
        return errtok(pos, "Double quote expected".to_string());
    }
    s += &substr;
    buf.read_set(SPACES);
    return newtok(pos, "string", s);
}

fn read_word(buf: &mut Buf) -> Token {
    let pos = buf.pos();
    let mut word = String::new();
    while buf.more() {
        let ch = buf.peek().unwrap();

        // Special form "calloc!"
        if word == "calloc" && ch == '!' {
            word.push(buf.get().unwrap());
            break;
        }

        if !ch.is_alphanumeric() && ch != '_' {
            break;
        }
        word.push(buf.get().unwrap());
    }

    if KEYWORDS.contains(&word.as_str()) {
        return newtok(pos, &word, String::new());
    }
    return newtok(pos, "word", word);
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
        return errtok(pos, "Single quote expected".to_string());
    }
    return newtok(pos, "char", s);
}

fn read_multiline_comment(buf: &mut Buf) -> Token {
    let pos = buf.pos();
    buf.skip_literal("/*");
    let comment = buf.until_literal("*/");
    if !buf.skip_literal("*/") {
        return errtok(pos, "*/ expected".to_string());
    }
    return newtok(pos, "comment", comment);
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
                input: "1e6",
                kind: "num",
                content: "1e6",
                pos: "1:1",
            },
            C {
                input: "3.917609E-2",
                kind: "num",
                content: "3.917609E-2",
                pos: "1:1",
            },
        ];

        for case in &cases {
            let t = _read_token(case.input);
            assert_eq!(t.content, case.content);
            assert_eq!(t.kind, case.kind);
            assert_eq!(t.pos.fmt(), case.pos);
        }

        for sym in SYMBOLS {
            let t = _read_token(format!("{}123", sym).as_str());
            assert_eq!(t.content, "");
            assert_eq!(t.kind, sym.to_string());
            assert_eq!(t.pos.fmt(), "1:1");
        }

        for kw in KEYWORDS {
            let t = _read_token(format!("{} 123", kw).as_str());
            assert_eq!(t.content, "");
            assert_eq!(t.kind, kw.to_string());
            assert_eq!(t.pos.fmt(), "1:1");
        }
    }
}

pub fn for_file(filename: &str) -> Result<Lexer, String> {
    return match fs::read_to_string(filename) {
        Ok(contents) => Ok(Lexer {
            source: crate::buf::new(contents),
            lookahead: VecDeque::new(),
        }),
        Err(e) => Err(format!("could not read {}: {}", filename, e)),
    };
}

pub struct Lexer {
    source: Buf,                // Source code buffer.
    lookahead: VecDeque<Token>, // Unget buffer.
}

impl Lexer {
    pub fn more(&mut self) -> bool {
        let r = self.get();
        if r.is_none() {
            return false;
        }
        self.unget(r.unwrap());
        return true;
    }

    pub fn pos(&self) -> Pos {
        self.source.pos()
    }

    pub fn get(&mut self) -> Option<Token> {
        if self.lookahead.len() > 0 {
            return self.lookahead.pop_front();
        }
        read_token_c(&mut self.source)
    }

    pub fn unget(&mut self, t: Token) {
        self.lookahead.push_front(t);
    }

    // Returns the next token or none.
    pub fn peek(&mut self) -> Option<&Token> {
        return self.peekn(1).map(|t| t[0]);
    }

    // Returns the next n tokens, or none if there is less than n tokens
    // remaining.
    pub fn peekn(&mut self, n: usize) -> Option<Vec<&Token>> {
        // Ensure that the lookahead buffer contains at least n tokens.
        while self.lookahead.len() < n {
            let r = read_token_c(&mut self.source);
            if r.is_none() {
                return None;
            }
            self.lookahead.push_back(r.unwrap());
        }
        let mut r = Vec::new();
        for i in 0..n {
            r.push(&self.lookahead[i])
        }
        return Some(r);
    }

    pub fn follows(&mut self, token_type: &str) -> bool {
        return self.more() && self.peek().unwrap().kind == token_type;
    }

    pub fn eat(&mut self, token_type: &str) -> bool {
        if self.follows(token_type) {
            self.get();
            return true;
        }
        return false;
    }
}
