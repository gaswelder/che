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
use std::io::BufRead;
use std::path::Path;
use std::process::{exit, Command, Stdio};
use std::string::String;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args[1] == "build" {
        match build(&args[2..]) {
            Ok(_) => exit(0),
            Err(m) => {
                eprintln!("{}", m);
                exit(1);
            }
        }
    }
    if args[1] == "deptree" {
        exit(deptree(&args[2..]));
    }
    exit(1);
}

#[test]
fn snapshots() {
    for entry in fs::read_dir("test/snapshots").unwrap() {
        let tmp = entry.unwrap().path();
        let path = tmp.to_str().unwrap();
        if path.ends_with(".out.c") {
            continue;
        }
        println!("{}", path);

        // get the output
        let m = parser::get_module(&path.to_string()).unwrap();
        let mods = resolve_deps(&m);
        let c_mods: Vec<nodes::CompatModule> =
            mods.iter().map(|m| translator::translate(&m)).collect();
        let result = format::format_compat_module(&c_mods[0]);

        let snapshot_path = path.replace(".c", ".out.c");
        if env::var("UPDATE_SNAPSHOT").is_ok() {
            fs::write(snapshot_path, result).unwrap();
        } else {
            assert_eq!(result, fs::read_to_string(snapshot_path).unwrap());
        }
    }
}

fn build(argv: &[String]) -> Result<(), String> {
    if argv.len() < 1 || argv.len() > 2 {
        return Err(String::from("Usage: build <source-path> [<output-path>]"));
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
    let m = parser::get_module(&path)?;

    // Get all modules that our module depends on. This includes our module as
    // well, so this is a complete list of modules required for the build.
    let mods = resolve_deps(&m);

    // Convert all modules to C modules.
    let c_mods: Vec<nodes::CompatModule> = mods.iter().map(|m| translator::translate(&m)).collect();

    // Save all C modules somewhere.
    let tmppath = format!("{}/tmp", parser::homepath());
    if fs::metadata(&tmppath).is_err() {
        fs::create_dir(&tmppath).unwrap();
    }
    struct PathId {
        path: String,
        id: String,
    }
    let mut paths: Vec<PathId> = Vec::new();
    for module in &c_mods {
        let src = format::format_compat_module(&module);
        let path = format!("{}/{:x}.c", tmppath, md5::compute(&module.id));
        paths.push(PathId {
            path: String::from(&path),
            id: String::from(&module.id),
        });
        fs::write(&path, &src).unwrap();
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
        "-Wno-parentheses",
        "-g",
    ]);
    for p in &paths {
        cmd.arg(p.path.clone());
    }
    cmd.args(&["-o", &name]);
    for l in link {
        cmd.args(&["-l", &l.trim()]);
    }
    cmd.stderr(Stdio::piped());
    let mut proc = cmd.spawn().unwrap();
    let out = proc.stderr.as_mut().unwrap();
    let reader = std::io::BufReader::new(out);
    for liner in reader.lines() {
        let mut line = liner.unwrap();
        for p in &paths {
            line = line.replace(&p.path, &format!("{}->{}", &p.id, &p.path));
        }
        println!("{}", &line);
    }
    let r = proc.wait().unwrap();
    exit(r.code().unwrap());
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

fn deptree(argv: &[String]) -> i32 {
    if argv.len() != 1 {
        eprintln!("usage: deptree <source-path>");
        return 1;
    }
    let path = &argv[0];
    let m = parser::get_module(&path).unwrap();
    let tree = build_tree(&m);
    let r = render_tree(&tree);
    println!("{}", r.join("\n"));
    return 0;
}

struct DepNode {
    name: String,
    deps: Vec<DepNode>,
}

fn build_tree(module: &nodes::Module) -> DepNode {
    let imports = module_imports(module);
    let mut deps = vec![];
    for import in imports {
        let m = parser::get_module(&import.path).unwrap();
        deps.push(build_tree(&m));
    }
    return DepNode {
        name: module.id.clone(),
        deps,
    };
}

fn render_tree(tree: &DepNode) -> Vec<String> {
    let mut lines: Vec<String> = vec![tree.name.clone()];
    let n = tree.deps.len();
    for (i, dep) in tree.deps.iter().enumerate() {
        let r = render_tree(&dep);
        if i == n - 1 {
            lines.append(&mut indent_tree(r, " └", "  "));
        } else {
            lines.append(&mut indent_tree(r, " ├", " │"));
        }
    }
    return lines;
}

fn indent_tree(lines: Vec<String>, first: &str, cont: &str) -> Vec<String> {
    let mut result: Vec<String> = Vec::new();
    for (i, line) in lines.iter().enumerate() {
        if i == 0 {
            result.push(format!("{}{}", first, line))
        } else {
            result.push(format!("{}{}", cont, line));
        }
    }
    return result;
}
