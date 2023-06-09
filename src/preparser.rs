use crate::lexer::{self, Lexer, Token};
use crate::resolve::{self};
use substring::Substring;

#[derive(Clone, Debug)]
pub struct ModuleImport {
    pub path: String, // import path, not normalized
    pub ns: String,   // alias (lib/foo.c -> foo)
}

#[derive(Clone, Debug)]
pub struct Ctx {
    pub path: String,
    pub typenames: Vec<String>,
    pub deps: Vec<Dep>,
}

impl Ctx {
    pub fn has_ns(&self, ns: &String) -> bool {
        for dep in &self.deps {
            if dep.ns == *ns {
                return true;
            }
        }
        return false;
    }
}

#[derive(Clone, Debug)]
pub struct Dep {
    // pub path: String,
    pub ns: String,
    pub typenames: Vec<String>,
}

#[derive(Debug)]
pub struct Preparse {
    pub imports: Vec<Imp1>,
    pub typenames: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct Imp1 {
    pub ns: String,
    pub path: String,
}

pub fn preparse(path: &String) -> Result<Preparse, String> {
    let mut typenames: Vec<String> = vec![];
    let mut imports: Vec<Imp1> = Vec::new();
    let mut l = lexer::for_file(path)?;
    loop {
        match l.get() {
            None => break,
            Some(t) => match t.kind.as_str() {
                "import" => {
                    let import_path = t.content.unwrap();
                    let basename = import_path.split("/").last().unwrap().to_string();
                    let ns = if import_path.ends_with(".c") {
                        basename.substring(0, basename.len() - 2).to_string()
                    } else {
                        basename
                    };
                    let res = resolve::resolve_import(path, &import_path)?;
                    imports.push(Imp1 {
                        ns: res.ns,
                        path: res.path,
                    });
                }
                "typedef" => {
                    // When a 'typedef' is encountered, look ahead to find the type name.
                    typenames.push(get_typename(&mut l)?);
                }
                _ => {
                    // The special #type hint tells for a fact that a type name exists
                    // without defining it. It's used in modules that interface with the
                    // OS headers.
                    if t.kind == "macro" && t.content.as_ref().unwrap().starts_with("#type") {
                        let name = t.content.unwrap()[6..].trim().to_string();
                        typenames.push(name);
                    }
                }
            },
        }
    }
    return Ok(Preparse { imports, typenames });
}

fn get_typename(lexer: &mut Lexer) -> Result<String, String> {
    // The type name is at the end of the typedef statement.
    // typedef foo bar;
    // typedef {...} bar;
    // typedef struct foo bar;

    if lexer.follows("{") {
        skip_brackets(lexer);
        let name = expect(lexer, "word", None).unwrap().content.unwrap();
        expect(lexer, ";", None)?;
        return Ok(name);
    }

    // Get all tokens until the semicolon.
    let mut buf: Vec<Token> = vec![];
    while !lexer.ended() {
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
            name = Some(t.content.unwrap());
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
            format!("expected '{}', got '{}' at {}", kind, next.kind, next.pos),
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
