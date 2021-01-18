use serde_json::{json, Value};
use std::env;
use std::io::stdin;
use std::string::String;
mod buf;
mod parser;
use buf::Buf;
use std::collections::HashMap;

fn main() {
    let args: Vec<String> = env::args().collect();
    let mut buf_instances: HashMap<String, Buf> = HashMap::new();
    if args.len() > 1 {
        serve_class(args[1].to_string(), &mut buf_instances);
    } else {
        serve_functions(&mut buf_instances);
    }
}

struct Call {
    f: String,
    args: Vec<serde_json::Value>,
}

fn read_call() -> Option<Call> {
    let mut buf = String::new();
    stdin().read_line(&mut buf).unwrap();
    if buf == "" {
        return None;
    }
    let v: Value = serde_json::from_str(&buf).unwrap();
    let f = v.as_object().unwrap().get("f").unwrap().as_str().unwrap();
    let args = v.as_object().unwrap().get("a").unwrap().as_array().unwrap();
    return Some(Call {
        f: f.to_string(),
        args: args.to_vec(),
    });
}

fn serve_class(class_name: String, buf_instances: &mut HashMap<String, Buf>) {
    if class_name != "buf" {
        panic!("unknown class name: {}", class_name);
    }

    let mut instance_key = String::new();
    loop {
        let callm = read_call();
        if callm.is_none() {
            break;
        }
        let call = callm.unwrap();
        let f = call.f;
        let args = call.args;
        if f != "__construct" {
            println!(
                "{}",
                json!({
                    "error": "__construct call expected",
                    "data": null
                })
            );
            continue;
        }
        let s = args[0].as_str().unwrap().to_string();
        instance_key = String::from("0xabcdef");
        buf_instances.insert(instance_key.clone(), buf::new(s));
        println!(
            "{}",
            json!({
                "error": "",
                "data": instance_key
            })
        );
        break;
    }

    // let mut b1 = b.unwrap();
    loop {
        let callm = read_call();
        if callm.is_none() {
            break;
        }
        let call = callm.unwrap();
        let f = call.f.as_str();
        let args = call.args;
        let b1 = buf_instances.get_mut(&instance_key).unwrap();
        let response = match f {
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
        println!("{}", response);
    }
}

fn serve_functions(buf_instances: &mut HashMap<String, Buf>) {
    loop {
        let callm = read_call();
        if callm.is_none() {
            break;
        }
        let call = callm.unwrap();
        let f = call.f.as_str();
        let args = call.args;
        let result = match f {
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
            "use_buf" => {
                let op = args[0].as_str().unwrap();
                match buf_instances.get_mut(op) {
                    Some(buf) => json!({
                        "error": "",
                        "data": buf.get()
                    }),
                    None => json!({
                        "error": format!("instance '{}' missing {:#?}", op, buf_instances.keys()),
                        "data": ""
                    }),
                }
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
}
