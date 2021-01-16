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
        route(f, args);
    }
}

fn route(f: &str, args: &Vec<serde_json::Value>) {
    let result = match f {
        "echo" => {
            let s = args[0].as_str().unwrap();
            json!({
                "error": "",
                "data": echo(s)
            })
        }
        "is_op" => {
            let token_type = args[0].as_str().unwrap();
            json!({
                "error": "",
                "data": parser::is_op(String::from(token_type))
            })
        }
        "is_prefix_op" => {
            let token_type = args[0].as_str().unwrap();
            json!({
                "error": "",
                "data": parser::is_prefix_op(String::from(token_type))
            })
        }
        "operator_strength" => {
            let op = args[0].as_str().unwrap();
            json!({
                "error": "",
                "data": parser::operator_strength(String::from(op))
            })
        }
        "is_postfix_op" => {
            let op = args[0].as_str().unwrap();
            json!({
                "error": "",
                "data": parser::is_postfix_op(String::from(op))
            })
        }
        _ => {
            json!({
                "error": "unknown function",
                "data": null
            })
        }
    };
    println!("{}", result);
}

fn echo(s: &str) -> String {
    return format!("echo: {}", s);
}
