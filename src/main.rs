use serde_json::{json, Value};
use std::string::String;
mod buf;
mod parser;
use buf::Buf;
use std::collections::HashMap;
use std::io::prelude::*;
use std::net::TcpListener;
mod lexer;

struct Call {
    ns: String,
    id: String,
    f: String,
    args: Vec<serde_json::Value>,
}

fn main() {
    let mut buf_instances: HashMap<String, Buf> = HashMap::new();

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

        let call = parse_call(String::from(s).trim()).unwrap();
        let response = if call.ns == "" {
            exec_function_call(call, &mut buf_instances)
        } else {
            exec_method_call(call, &mut buf_instances)
        };
        c.write(format!("{}", response).as_bytes()).unwrap();
        c.flush().unwrap();
    }
}

fn parse_call(buf: &str) -> Option<Call> {
    if buf == "" {
        return None;
    }
    let v: Value = serde_json::from_str(&buf).unwrap();
    let obj = v.as_object().unwrap();
    return Some(Call {
        ns: obj.get("ns").unwrap().as_str().unwrap().to_string(),
        id: obj.get("id").unwrap().as_str().unwrap().to_string(),
        f: obj.get("f").unwrap().as_str().unwrap().to_string(),
        args: obj.get("a").unwrap().as_array().unwrap().to_vec(),
    });
}

fn exec_method_call(call: Call, buf_instances: &mut HashMap<String, Buf>) -> serde_json::Value {
    if call.ns != "buf" {
        panic!("unknown class name: {}", call.ns);
    }

    let f = call.f.as_str();
    let args = call.args;

    if f == "__construct" {
        let instance_key = format!("#{}", buf_instances.len());
        println!("new buf {}", instance_key);
        let s = args[0].as_str().unwrap().to_string();
        buf_instances.insert(instance_key.clone(), buf::new(s));
        return json!({
            "error": "",
            "data": instance_key
        });
    }
    let b1 = buf_instances.get_mut(&call.id).unwrap();
    return match f {
        "ended" => json!({
            "error": "",
            "data": b1.ended()
        }),
        "more" => json!({
            "error": "",
            "data": b1.more()
        }),
        "pos" => json!({
            "error": "",
            "data": b1.pos()
        }),
        "peek" => {
            let r = b1.peek();
            json!({
                "error": "",
                "data": r
            })
        }
        "get" => json!({
            "error": "",
            "data": b1.get()
        }),
        "unget" => json!({
            "error": "",
            "data": b1.unget(args[0].as_str().unwrap().chars().next().unwrap())
        }),
        "read_set" => {
            let set = String::from(args[0].as_str().unwrap());
            json!({
                "error": "",
                "data": b1.read_set(set)
            })
        }
        "skip_literal" => json!({
            "error": "",
            "data": b1.skip_literal(args[0].as_str().unwrap())
        }),
        "literal_follows" => json!({
            "error": "",
            "data": b1.literal_follows(args[0].as_str().unwrap())
        }),
        "until_literal" => json!({
            "error": "",
            "data": b1.until_literal(args[0].as_str().unwrap())
        }),
        "skip_until" => json!({
            "error": "",
            "data": b1.skip_until(args[0].as_str().unwrap().chars().next().unwrap())
        }),
        "context" => json!({
            "error": "",
            "data": b1.context()
        }),
        _ => {
            panic!("unknown method: {}", f);
        }
    };
}

fn exec_function_call(call: Call, buf_instances: &mut HashMap<String, Buf>) -> serde_json::Value {
    let f = call.f.as_str();
    let args = call.args;
    return match f {
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
        "is_type" => {
            let op = args[0].as_str().unwrap();
            let mut typenames: Vec<String> = Vec::new();
            for s in args[1].as_array().unwrap() {
                typenames.push(s.as_str().unwrap().to_string());
            }
            json!({
                "error": "",
                "data": parser::is_type(op.to_string(), &typenames)
            })
        }
        "read_file" => {
            let filename = args[0].as_str().unwrap();
            return match lexer::read_file(filename) {
                Err(s) => json!({
                    "error": s,
                    "data": null
                }),
                Ok(tokens) => json!({
                    "error": "",
                    "data": tokens
                }),
            };
        }
        "read_token" => {
            let buf = buf_instances.get_mut(args[0].as_str().unwrap()).unwrap();
            return json!({
                "error": "",
                "data": lexer::read_token(buf)
            });
        }
        _ => {
            json!({
                "error": format!("unknown function: {}", f),
                "data": null
            })
        }
    };
}
