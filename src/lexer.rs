use crate::buf::Buf;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize)]
pub struct Token {
    pub kind: String,
    pub content: Option<String>,
    pub pos: String,
}

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
