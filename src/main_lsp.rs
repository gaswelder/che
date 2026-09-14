use serde_json::{json, Value};
use std::io::{BufRead, BufReader, Read, Write};
use std::process::exit;

pub fn run(_args: &[String]) -> i32 {
    let stdin = std::io::stdin();
    let stdout = std::io::stdout();
    let mut reader = BufReader::new(stdin);
    let mut out = stdout.lock();
    loop {
        let mut content_length = None;
        loop {
            let mut line = String::new();
            let n = match reader.read_line(&mut line) {
                Ok(n) => n,
                Err(e) => {
                    eprintln!("lsp: error reading headers: {}", e);
                    return 0;
                }
            };
            if n == 0 {
                eprintln!("lsp: stdin closed, exiting");
                return 0;
            }
            let trimmed = line.trim_end();
            if trimmed.is_empty() {
                break;
            }
            if trimmed.starts_with("Content-Length:") {
                content_length = match trimmed["Content-Length:".len()..].trim().parse::<usize>() {
                    Ok(length) => Some(length),
                    Err(e) => {
                        eprintln!("lsp: bad Content-Length in {:?}: {}", trimmed, e);
                        None
                    }
                };
            }
        }
        let length = match content_length {
            Some(length) => length,
            None => {
                eprintln!("lsp: message missing Content-Length header, skipping");
                continue;
            }
        };
        let mut body = vec![0u8; length];
        if let Err(e) = reader.read_exact(&mut body) {
            eprintln!("lsp: error reading body: {}", e);
            return 0;
        }
        let msg = match serde_json::from_slice::<Value>(&body) {
            Ok(msg) => msg,
            Err(e) => {
                eprintln!("lsp: failed to parse message: {}", e);
                eprintln!("lsp: raw body: {:?}", String::from_utf8_lossy(&body));
                continue;
            }
        };
        handle(&msg, &mut out);
    }
}

fn handle(msg: &Value, out: &mut impl Write) {
    let method = msg["method"].as_str();
    match msg.get("id") {
        Some(id) => {
            eprintln!(
                "lsp: request `{}` id={}",
                method.unwrap_or("<no method>"),
                id
            );
            match method {
                Some("initialize") => {
                    let result = json!({
                        "capabilities": {
                            "textDocumentSync": 1,
                            "hoverProvider": false,
                            "declarationProvider": false,
                        },
                        "serverInfo": { "name": "che-lsp", "version": "0.1.0" },
                    });
                    send(out, id, &result);
                }
                Some("shutdown") => {
                    send(out, id, &Value::Null);
                }
                _ => {
                    eprintln!("lsp: request `{}` not supported", method.unwrap_or(""));
                    let error = json!({ "code": -32601, "message": "method not found" });
                    send_error(out, id, &error);
                }
            }
        }
        None => {
            eprintln!("lsp: notification `{}`", method.unwrap_or("<no method>"));
            if method == Some("exit") {
                eprintln!("lsp: received exit, shutting down");
                exit(0);
            }
        }
    }
}

fn send(out: &mut impl Write, id: &Value, result: &Value) {
    let body = serde_json::to_string(&json!({
        "jsonrpc": "2.0",
        "id": id,
        "result": result,
    }))
    .unwrap();
    eprintln!("lsp: sending response id={} result={}", id, body);
    write!(out, "Content-Length: {}\r\n\r\n{}", body.len(), body).unwrap();
    out.flush().unwrap();
}

fn send_error(out: &mut impl Write, id: &Value, error: &Value) {
    let body = serde_json::to_string(&json!({
        "jsonrpc": "2.0",
        "id": id,
        "error": error,
    }))
    .unwrap();
    eprintln!("lsp: sending error id={} error={}", id, body);
    write!(out, "Content-Length: {}\r\n\r\n{}", body.len(), body).unwrap();
    out.flush().unwrap();
}