use serde::Deserialize;
use serde_json::json;
use std::string::String;
mod buf;
mod parser;
use std::collections::HashMap;
use std::io::prelude::*;
use std::net::TcpListener;
mod lexer;
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
    let mut read_files: HashMap<String, Vec<Token>> = HashMap::new();

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
            exec_function_call(call, &mut read_files, &mut lexer_instances)
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

fn exec_function_call(
    call: Call,
    read_files: &mut HashMap<String, Vec<Token>>,
    lexer_instances: &mut HashMap<String, Lexer>,
) -> serde_json::Value {
    let f = call.f.as_str();
    let args = call.a;
    return match f {
        "is_op" => {
            let token_type = args[0].as_str().unwrap();
            json!({
                "error": "",
                "data": parser::is_op(token_type)
            })
        }
        "is_prefix_op" => {
            let token_type = args[0].as_str().unwrap();
            json!({
                "error": "",
                "data": parser::is_prefix_op(token_type)
            })
        }
        "operator_strength" => {
            let op = args[0].as_str().unwrap();
            json!({
                "error": "",
                "data": parser::operator_strength(op)
            })
        }
        "is_postfix_op" => {
            let op = args[0].as_str().unwrap();
            json!({
                "error": "",
                "data": parser::is_postfix_op(op)
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
                "data": parser::is_type(op, &typenames)
            })
        }
        "expect" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            let kind = args[1].as_str().unwrap();
            let comment = args[2].as_str();
            match parser::expect(lexer, kind, comment) {
                Ok(tok) => json!({
                    "error": "",
                    "data": tok
                }),
                Err(s) => json!({
                    "error": s,
                    "data": null
                }),
            }
        }
        "parse_identifier" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            match parser::parse_identifier(lexer) {
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
        "parse_statement" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            let mut typenames: Vec<String> = Vec::new();
            for item in args[1].as_array().unwrap() {
                typenames.push(item.as_str().unwrap().to_string())
            }
            match parser::parse_statement(lexer, &typenames) {
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
        "parse_form" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            let mut typenames: Vec<String> = Vec::new();
            for item in args[1].as_array().unwrap() {
                typenames.push(item.as_str().unwrap().to_string())
            }
            match parser::parse_form(lexer, &typenames) {
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
        "parse_function_parameter" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            let mut typenames: Vec<String> = Vec::new();
            for item in args[1].as_array().unwrap() {
                typenames.push(item.as_str().unwrap().to_string())
            }
            match parser::parse_function_parameter(lexer, &typenames) {
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
        "parse_union" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            let mut typenames: Vec<String> = Vec::new();
            for item in args[1].as_array().unwrap() {
                typenames.push(item.as_str().unwrap().to_string())
            }
            match parser::parse_union(lexer, &typenames) {
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
        "parse_module_object" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            let mut typenames: Vec<String> = Vec::new();
            for item in args[1].as_array().unwrap() {
                typenames.push(item.as_str().unwrap().to_string())
            }
            match parser::parse_module_object(lexer, &typenames) {
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
        "parse_import" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            match parser::parse_import(lexer) {
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
        "parse_compat_macro" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            match parser::parse_compat_macro(lexer) {
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
        "parse_sizeof" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            let mut typenames: Vec<String> = Vec::new();
            for item in args[1].as_array().unwrap() {
                typenames.push(item.as_str().unwrap().to_string())
            }
            match parser::parse_sizeof(lexer, &typenames) {
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
        "parse_expression" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            let mut typenames: Vec<String> = Vec::new();
            for item in args[1].as_array().unwrap() {
                typenames.push(item.as_str().unwrap().to_string())
            }
            let mut current_strength1 = args[2].as_u64().unwrap();
            let mut current_strength: usize = 0;
            while current_strength1 > 0 {
                current_strength += 1;
                current_strength1 -= 1;
            }
            match parser::parse_expression(lexer, current_strength, &typenames) {
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
        "parse_type" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            let comment = args[1].as_str();
            match parser::parse_type(lexer, comment) {
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
        "parse_anonymous_typeform" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            match parser::parse_anonymous_typeform(lexer) {
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
        "parse_anonymous_parameters" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            match parser::parse_anonymous_parameters(lexer) {
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
        "parse_literal" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            match parser::parse_literal(lexer) {
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
        "parse_array_literal" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            match parser::parse_array_literal(lexer) {
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
        "parse_array_literal_entry" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            match parser::parse_array_literal_entry(lexer) {
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
        "parse_enum" => {
            let instance_id = String::from(args[0].as_str().unwrap());
            let lexer = lexer_instances.get_mut(&instance_id).unwrap();
            let is_pub = args[1].as_bool().unwrap();
            match parser::parse_enum(lexer, is_pub) {
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
        "read_file" => {
            let filename = args[0].as_str().unwrap();
            if read_files.get(filename).is_none() {
                println!("reading {}", filename);
                let r = lexer::read_file(filename);
                if r.is_err() {
                    return json!({
                        "error": r.unwrap_err(),
                        "data": null
                    });
                }
                read_files.insert(filename.to_string(), r.unwrap());
            }
            return json!({
                "error": "",
                "data": read_files.get(filename).unwrap()
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
