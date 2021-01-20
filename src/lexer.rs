use crate::buf::Buf;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize)]
pub struct Token {
    pub kind: String,
    pub content: Option<String>,
    pub pos: String,
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
