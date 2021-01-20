use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize)]
pub struct Token {
    pub kind: String,
    pub content: Option<String>,
    pub pos: String,
}
