use crate::checkers;
use crate::format;
use crate::nodes;
use crate::parser;
use crate::translator;
use md5;
use std::fs;
use std::io::BufRead;
use std::path::Path;
use std::process::{Command, Stdio};

pub struct Import {
    pub path: String,
}

fn basename(path: &String) -> String {
    return String::from(Path::new(path).file_name().unwrap().to_str().unwrap());
}

pub fn build_prog(source_path: &String, output_name: &String) -> Result<(), String> {
    // Decide where we'll stash all generated C code.
    let tmp_dir_path = format!("{}/tmp", parser::homepath());
    if fs::metadata(&tmp_dir_path).is_err() {
        fs::create_dir(&tmp_dir_path).unwrap();
    }

    let c_modules = translate(source_path)?;
    let pathsmap = write_c99(&c_modules, &tmp_dir_path).unwrap();

    // Determine the list of OS libraries to link. "m" is the library for code
    // in included <math.h>. For simplicity both the header and the library are
    // always included.
    // Other libraries are simply taked from the #link hints in the source code.
    let mut link: Vec<String> = vec!["m".to_string()];
    for module in &c_modules {
        for l in &module.link {
            if link.iter().position(|x| x == l).is_none() {
                link.push(l.clone());
            }
        }
    }

    let mut cmd = Command::new("c99");
    cmd.args(&[
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic",
        "-pedantic-errors",
        "-fmax-errors=1",
        "-Wno-parentheses",
        "-g",
    ]);
    for p in &pathsmap {
        cmd.arg(p.path.clone());
    }
    cmd.args(&["-o", &output_name]);
    for l in link {
        cmd.args(&["-l", &l.trim()]);
    }
    cmd.stderr(Stdio::piped());
    let mut proc = cmd.spawn().unwrap();
    let out = proc.stderr.as_mut().unwrap();
    let reader = std::io::BufReader::new(out);
    for liner in reader.lines() {
        let mut line = liner.unwrap();
        for p in &pathsmap {
            line = line.replace(&p.path, &format!("{} -> {}", &p.id, &p.path));
        }
        println!("{}", &line);
    }
    let r = proc.wait().unwrap();
    if r.success() {
        return Ok(());
    }
    return Err(String::from("build failed"));
}

pub fn translate(source_path: &String) -> Result<Vec<nodes::CompatModule>, String> {
    // Get the module we're building.
    let main_module = parser::get_module(&basename(source_path), &source_path)?;

    // Get all modules that our module depends on. This includes our module as
    // well, so this is a complete list of modules required for the build.
    let all_modules = resolve_deps(&main_module);

    // Dome some checks.
    for m in &all_modules {
        for imp in module_imports(m) {
            let dep = parser::get_module(&imp.path, &m.source_path)?;
            if !checkers::depused(&m, &dep) {
                return Err(format!("{}: imported {} is not used", m.id, dep.id));
            }
        }
    }
    return Ok(all_modules
        .iter()
        .map(|m| translator::translate(&m))
        .collect());
}

pub struct PathId {
    path: String,
    id: String,
}

pub fn write_c99(
    c_modules: &Vec<nodes::CompatModule>,
    dirpath: &String,
) -> Result<Vec<PathId>, String> {
    // Write the generated C source files in the temp directory and build the
    // mapping of the generated C file path to the original source file path,
    // that will be used to trace C compiler's errors at least to the original
    // files.
    let mut paths: Vec<PathId> = Vec::new();
    for module in c_modules {
        let src = format::format_module(&module);
        let path = format!("{}/{:x}.c", dirpath, md5::compute(&module.id));
        paths.push(PathId {
            path: String::from(&path),
            id: String::from(&module.id),
        });
        fs::write(&path, &src).unwrap();
    }
    return Ok(paths);
}

// Returns a list of import nodes from the module.
pub fn module_imports(m: &nodes::Module) -> Vec<Import> {
    let mut list: Vec<Import> = vec![];
    for element in &m.elements {
        match element {
            nodes::ModuleObject::Import { path } => list.push(Import { path: path.clone() }),
            _ => {}
        }
    }
    return list;
}

// Returns a list of all modules required to build the given module, including
// the same module.
pub fn resolve_deps(m: &nodes::Module) -> Vec<nodes::Module> {
    let mut deps: Vec<nodes::Module> = vec![m.clone()];
    let mut present = vec![m.id.clone()];
    for imp in module_imports(&m) {
        let sub = parser::get_module(&imp.path, &m.source_path).unwrap();
        for dep in resolve_deps(&sub) {
            if present.contains(&&dep.id) {
                continue;
            }
            present.push(dep.id.clone());
            deps.push(dep);
        }
    }
    return deps;
}
