mod buf;
mod format;
mod lexer;
mod nodes;
mod parser;
mod translator;
use md5;
#[cfg(test)]
use pretty_assertions::assert_eq;
use std::env;
use std::fs;
use std::path::Path;
use std::process::Command;
use std::string::String;

fn main() {
    let args: Vec<String> = env::args().collect();
    println!("{:?}", args);

    if args[1] == "build" {
        build(&args[2..]);
        return;
    }
}

#[test]
fn one() {
    let m = parser::get_module(&"test/snapshots/brace.in.c".to_string()).unwrap();
    let mods = resolve_deps(&m);
    let c_mods: Vec<nodes::CompatModule> = mods.iter().map(|m| translator::translate(&m)).collect();
    let result = format::format_compat_module(&c_mods[0]);
    assert_eq!(
        result,
        fs::read_to_string("test/snapshots/brace.out.c").unwrap()
    );
}

fn build(argv: &[String]) {
    if argv.len() < 1 || argv.len() > 2 {
        eprintln!("Usage: build <source-path> [<output-path>]\n");
        return;
    }
    let path = &argv[0];
    let name = if argv.len() == 2 {
        argv[1].clone()
    } else {
        Path::new(&path)
            .with_extension("")
            .file_name()
            .unwrap()
            .to_str()
            .unwrap()
            .to_string()
    };

    // Get the module we're building.
    let m = parser::get_module(&path).unwrap();

    // Get all modules that our module depends on. This includes our module as
    // well, so this is a complete list of modules required for the build.
    let mods = resolve_deps(&m);

    // Convert all modules to C modules.
    let c_mods: Vec<nodes::CompatModule> = mods.iter().map(|m| translator::translate(&m)).collect();

    // Save all C modules somewhere.
    if fs::metadata("tmp").is_err() {
        fs::create_dir("tmp").unwrap();
    }
    let mut paths: Vec<String> = Vec::new();
    for module in &c_mods {
        let src = format::format_compat_module(&module);
        let path = format!("tmp/{:x}.c", md5::compute(&module.id));
        fs::write(&path, &src).unwrap();
        paths.push(path);
    }

    let mut link: Vec<String> = vec!["m".to_string()];
    for module in &c_mods {
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
        "-fmax-errors=3",
        "-g",
    ]);
    for p in paths {
        cmd.arg(p);
    }
    cmd.args(&["-o", &name]);
    for l in link {
        cmd.args(&["-l", &l]);
    }
    println!("{:?}", cmd);
    let mut ch = cmd.spawn().unwrap();
    let r = ch.wait().unwrap();
    println!("exit {}", r);
}

fn resolve_deps(m: &nodes::Module) -> Vec<nodes::Module> {
    let mut deps: Vec<nodes::Module> = vec![m.clone()];
    let mut present = vec![m.id.clone()];
    for imp in module_imports(&m) {
        let sub = parser::get_module(&imp.path).unwrap();
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

fn module_imports(module: &nodes::Module) -> Vec<nodes::Import> {
    let mut list: Vec<nodes::Import> = vec![];
    for element in &module.elements {
        match element {
            nodes::ModuleObject::Import(x) => list.push(x.clone()),
            _ => {}
        }
    }
    return list;
}
