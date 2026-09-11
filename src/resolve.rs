use std::env;
use std::path::Path;
use substring::Substring;

#[derive(Clone, Debug)]
pub struct ModuleRef {
    pub path: String, // Path to the module in the file system
    pub ns: String,   // Prefix that the importing module will use
    pub suffix: String,
}

// Returns the directory where all built-in libraries are stored.
pub fn homepath() -> String {
    return env::var("CHELANG_HOME").unwrap_or(String::from("."));
}

pub fn resolve_import(base_path: &str, name: &str) -> Result<ModuleRef, String> {
    // If requested module name ends with ".c", we look for it relative to
    // base_path (importing module's location). If ".c" is omitted, we look for
    // it inside the lib directory.

    let mut paths = Vec::new();

    if name.ends_with(".c") {
        let p = Path::new(base_path).parent().unwrap().join(name);
        paths.push(String::from(p.to_str().unwrap()));
        paths.push(String::from(p.to_str().unwrap()).replace(".c", ".unix.c"));
    } else {
        paths.push(format!("{}/lib/{}.c", homepath(), name))
    };

    for path in &paths {
        if std::fs::metadata(&path).is_ok() {
            let bn = basename(&path);
            let mut ns = bn.substring(0, bn.len() - 2).to_string();
            let mut suffix = String::new();
            if ns.ends_with(".unix") {
                ns = ns.replace(".unix", "");
                suffix = String::from("unix");
            }
            return Ok(ModuleRef {
                path: path.clone(),
                ns,
                suffix,
            });
        }
    }
    return Err(format!(
        "can't find module '{}' (looked at {})",
        name,
        paths.join(", ")
    ));
}

pub fn basename(s: &str) -> String {
    return s.split("/").last().unwrap().to_string();
}
