use std::env;
use std::path::Path;
use substring::Substring;

#[derive(Clone, Debug)]
pub struct Resolve {
    pub path: String,
    pub ns: String,
}

pub fn resolve_import(base_path: &String, name: &String) -> Result<Resolve, String> {
    // If requested module name ends with ".c", we look for it relative to
    // base_path (importing module's location). If ".c" is omitted, we look for
    // it inside the lib directory.
    let path = if name.ends_with(".c") {
        let p = Path::new(base_path).parent().unwrap().join(name);
        String::from(p.to_str().unwrap())
    } else {
        format!("{}/lib/{}.c", homepath(), name)
    };
    if std::fs::metadata(&path).is_err() {
        return Err(format!("can't find module '{}' (looked at {})", name, path));
    }
    let bn = basename(&path);
    let ns = bn.substring(0, bn.len() - 2).to_string();
    return Ok(Resolve { path, ns });
}

pub fn getns(name: &String) -> String {
    let bn = basename(name);
    return if bn.ends_with(".c") {
        bn.substring(0, bn.len() - 2).to_string()
    } else {
        bn
    };
}

fn basename(s: &String) -> String {
    return s.split("/").last().unwrap().to_string();
}

pub fn homepath() -> String {
    return env::var("CHELANG_HOME").unwrap_or(String::from("."));
}
