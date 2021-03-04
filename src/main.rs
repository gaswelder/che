use serde::Deserialize;
use serde_json::json;
use std::string::String;
mod buf;
mod parser;
use std::collections::HashMap;
use std::io::prelude::*;
use std::net::TcpListener;
mod lexer;
mod nodes;
use lexer::Lexer;
use lexer::Token;

#[derive(Deserialize)]
struct Call {
    ns: String,
    id: String,
    f: String,
    a: Vec<serde_json::Value>,
}

fn main() {
    let mut lexer_instances: HashMap<String, Lexer> = HashMap::new();

    let ln = TcpListener::bind("localhost:2124").unwrap();
    for conn in ln.incoming() {
        let mut c = conn.unwrap();
        let mut s = String::new();
        loop {
            let mut input = [0; 20 * 1024];
            let len = c.read(&mut input).unwrap();
            if len == 0 {
                break;
            }
            s.push_str(String::from_utf8(input[0..len].to_vec()).unwrap().as_str());
            if input[len - 1] == '\n' as u8 {
                break;
            }
        }

        let call: Call = serde_json::from_str(String::from(s).trim()).unwrap();
        let response = if call.ns == "" {
            exec_function_call(call)
        } else {
            exec_method_call(call, &mut lexer_instances)
        };
        c.write(format!("{}", response).as_bytes()).unwrap();
        c.flush().unwrap();
    }
}

fn exec_method_call(call: Call, lexer_instances: &mut HashMap<String, Lexer>) -> serde_json::Value {
    if call.ns != "lexer" {
        panic!("unknown class name: {}", call.ns);
    }

    let f = call.f.as_str();
    let args = call.a;

    if f == "__construct" {
        let instance_key = format!("#{}", lexer_instances.len());
        println!("new {} {}", call.ns, instance_key);
        let s = args[0].as_str().unwrap();
        lexer_instances.insert(instance_key.clone(), lexer::new(s));
        return json!({
            "error": "",
            "data": instance_key
        });
    }
    let b1 = lexer_instances.get_mut(&call.id).unwrap();
    return match f {
        "ended" => json!({
            "error": "",
            "data": b1.ended()
        }),
        "more" => json!({
            "error": "",
            "data": b1.more()
        }),
        "get" => json!({
            "error": "",
            "data": b1.get()
        }),
        "unget" => {
            let c = args[0].as_object().unwrap().get("content").unwrap();
            let t = Token {
                kind: args[0]
                    .as_object()
                    .unwrap()
                    .get("kind")
                    .unwrap()
                    .as_str()
                    .unwrap()
                    .to_string(),
                content: if c.is_null() {
                    None
                } else {
                    Some(c.as_str().unwrap().to_string())
                },
                pos: args[0]
                    .as_object()
                    .unwrap()
                    .get("pos")
                    .unwrap()
                    .as_str()
                    .unwrap()
                    .to_string(),
            };
            b1.unget(t);
            json!({
                "error": "",
                "data": null
            })
        }
        "peek" => {
            json!({
                "error": "",
                "data": b1.peek()
            })
        }
        "peek_n" => {
            json!({
                "error": "",
                "data": b1.peek_n(args[0].as_u64().unwrap() as usize)
            })
        }
        "follows" => {
            json!({
                "error": "",
                "data": b1.follows(args[0].as_str().unwrap())
            })
        }
        _ => {
            panic!("unknown method: {}", f);
        }
    };
}

fn exec_function_call(call: Call) -> serde_json::Value {
    let f = call.f.as_str();
    let args = call.a;
    return match f {
        "operator_strength" => {
            let op = args[0].as_str().unwrap();
            json!({
                "error": "",
                "data": parser::operator_strength(op)
            })
        }
        "get_module" => {
            let name = args[0].as_str().unwrap();
            let r = parser::get_module(name);
            match r {
                Ok(node) => json!({
                    "error": "",
                    "data": node
                }),
                Err(s) => json!({
                    "error": s,
                    "data": null
                }),
            }
        }
        _ => {
            json!({
                "error": format!("unknown function: {}", f),
                "data": null
            })
        }
    };
}
