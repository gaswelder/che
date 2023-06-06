use std::env;
use std::path::Path;

pub fn resolve_import(base_path: &String, name: &String) -> Result<String, String> {
    // If requested module name ends with ".c", we look for it relative to
    // base_path (importing module's location). If ".c" is omitted, we look for
    // it inside the lib directory.
    let module_path = if name.ends_with(".c") {
        let p = Path::new(base_path).parent().unwrap().join(name);
        String::from(p.to_str().unwrap())
    } else {
        format!("{}/lib/{}.c", homepath(), name)
    };
    if std::fs::metadata(&module_path).is_err() {
        return Err(format!(
            "can't find module '{}' (looked at {})",
            name, module_path
        ));
    }
    return Ok(module_path);
}

pub fn homepath() -> String {
    return env::var("CHELANG_HOME").unwrap_or(String::from("."));
}
