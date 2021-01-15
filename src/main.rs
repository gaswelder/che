use serde_json::{json, Value};
use std::io::stdin;
use std::string::String;
mod parser;

fn main() {
    loop {
        let mut buf = String::new();
        stdin().read_line(&mut buf).unwrap();
        if buf == "" {
            break;
        }
        let v: Value = serde_json::from_str(&buf).unwrap();
        let f = v.as_object().unwrap().get("f").unwrap().as_str().unwrap();
        let args = v.as_object().unwrap().get("a").unwrap().as_array().unwrap();
        if f == "echo" {
            let s = args[0].as_str().unwrap();
            println!(
                "{}",
                json!({
                    "error": "",
                    "data": echo(s)
                })
            )
        } else if f == "is_op" {
            let token_type = args[0].as_str().unwrap();
            println!(
                "{}",
                json!({
                    "error": "",
                    "data": parser::is_op(String::from(token_type))
                })
            )
        } else if f == "operator_strength" {
            let op = args[0].as_str().unwrap();
            println!(
                "{}",
                json!({
                    "error": "",
                    "data": parser::operator_strength(String::from(op))
                })
            )
        } else if f == "is_postfix_op" {
            let op = args[0].as_str().unwrap();
            println!(
                "{}",
                json!({
                    "error": "",
                    "data": parser::is_postfix_op(String::from(op))
                })
            )
        } else {
            println!(
                "{}",
                json!({
                    "error": "unknown function",
                    "data": null
                })
            )
        }
    }
}

fn echo(s: &str) -> String {
    return format!("echo: {}", s);
}
