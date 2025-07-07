use crate::lexer::{self, Lexer, Token};
use crate::resolve::{self, ModuleRef};

#[derive(Debug, Clone)]
pub struct ModuleInfo {
    // References to other modules that this module contains.
    pub imports: Vec<ModuleRef>,

    // Types that this module declares,
    // both exported and non-exported.
    pub typedefs: Vec<String>,

    // Path where this module is located.
    pub filepath: String,

    pub uniqid: String,
}

pub fn preparse(path: &str) -> Result<ModuleInfo, String> {
    let mut typenames: Vec<String> = vec![];
    let mut imports: Vec<ModuleRef> = Vec::new();
    let mut l = lexer::for_file(path)?;
    loop {
        match l.get() {
            None => break,
            Some(t) => match t.kind.as_str() {
                "error" => {
                    return Err(format!("{} at {}:{}", t.content, path, t.pos.fmt()));
                }
                "import" => {
                    let res = resolve::resolve_import(path, &t.content)?;
                    imports.push(res);
                }
                "typedef" => {
                    // When a 'typedef' is encountered, look ahead to find the type name.
                    typenames.push(get_typename(&mut l)?);
                }
                _ => {
                    // The special #type hint declares that a type name exists
                    // without defining it. These hints are needed in modules
                    // that interface with the OS headers.
                    if t.kind == "macro" && t.content.starts_with("#type") {
                        let name = t.content[6..].trim().to_string();
                        typenames.push(name);
                    }
                }
            },
        }
    }
    return Ok(ModuleInfo {
        imports,
        typedefs: typenames,
        filepath: String::from(path),
        uniqid: format!(
            "ns_{}",
            path.replace("/", "_").replace(".", "_").replace("-", "_")
        ),
    });
}

fn get_typename(lexer: &mut Lexer) -> Result<String, String> {
    // The type name is at the end of the typedef statement.
    // typedef foo bar;
    // typedef {...} bar;
    // typedef struct foo bar;

    if lexer.follows("{") {
        skip_brackets(lexer);
        let name = expect(lexer, "word", None).unwrap().content;
        expect(lexer, ";", None)?;
        return Ok(name);
    }

    // Get all tokens until the semicolon.
    let mut buf: Vec<Token> = vec![];
    while lexer.more() {
        let t = lexer.get().unwrap();
        if t.kind == ";" {
            break;
        }
        buf.push(t);
    }

    if buf.is_empty() {
        return Err("No tokens after 'typedef'".to_string());
    }

    buf.reverse();

    // We assume that function typedefs end with "(...)".
    // In that case we omit that part.
    if buf[0].kind == ")" {
        while !buf.is_empty() {
            let t = buf.remove(0);
            if t.kind == "(" {
                break;
            }
        }
    }

    let mut name: Option<String> = None;

    // The last "word" token is assumed to be the type name.
    while !buf.is_empty() {
        let t = buf.remove(0);
        if t.kind == "word" {
            name = Some(t.content);
            break;
        }
    }

    if name.is_none() {
        return Err("Type name expected in the typedef".to_string());
    }
    return Ok(name.unwrap());
}

fn skip_brackets(lexer: &mut Lexer) {
    expect(lexer, "{", None).unwrap();
    while lexer.more() {
        if lexer.follows("{") {
            skip_brackets(lexer);
            continue;
        }
        if lexer.follows("}") {
            break;
        }
        lexer.get();
    }
    expect(lexer, "}", None).unwrap();
}

fn expect(lexer: &mut Lexer, kind: &str, comment: Option<&str>) -> Result<Token, String> {
    if !lexer.more() {
        return Err(with_comment(
            comment,
            format!("expected '{}', got end of file", kind),
        ));
    }
    let next = lexer.peek().unwrap();
    if next.kind != kind {
        return Err(with_comment(
            comment,
            format!(
                "expected '{}', got '{}' at {}",
                kind,
                next.kind,
                next.pos.fmt()
            ),
        ));
    }
    return Ok(lexer.get().unwrap());
}

fn with_comment(comment: Option<&str>, message: String) -> String {
    match comment {
        Some(s) => format!("{}: {}", s, message),
        None => message.to_string(),
    }
}
