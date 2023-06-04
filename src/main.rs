mod buf;
mod build;
mod checkers;
mod format;
mod lexer;
mod main_test;
mod nodes;
mod parser;
#[cfg(test)]
mod rename;
mod translator;
#[cfg(test)]
use pretty_assertions::assert_eq;
use std::env;
#[cfg(test)]
use std::fs;
use std::path::Path;
use std::process::exit;
use std::str;
use std::string::String;

#[test]
fn test_rename() {
    let mut m = parser::get_module(&"json".to_string(), &String::from(".")).unwrap();
    let exports = checkers::get_exports(&m);
    let mut names: Vec<String> = Vec::new();
    for e in exports.consts {
        names.push(e.id);
    }
    for f in exports.fns {
        names.push(f.form.name);
    }
    for t in exports.types {
        names.push(t.form.alias);
    }
    println!("{:?}", names);
    rename::rename_mod(&mut m, &"kek".to_string(), &names);
    let c = translator::translate(&m);
    fs::write("out.c", format::format_compat_module(&c)).unwrap();
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() == 1 {
        usage();
    }
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
    if args[1] == "exports" {
        exit(exports(&args[2..]));
    }
    if args[1] == "test" {
        match main_test::run_tests(&args[2..]) {
            Err(s) => {
                eprintln!("{}", s);
                exit(1);
            }
            Ok(_) => {
                exit(0);
            }
        }
    }
    usage();
}

fn usage() {
    eprintln!("usage: che build | deptree | exports | test");
    exit(1);
}

fn basename(path: &String) -> String {
    return String::from(Path::new(path).file_name().unwrap().to_str().unwrap());
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
        let p = &path.to_string();
        let m = parser::get_module(&basename(p), &p).unwrap();
        let mods = build::resolve_deps(&m);
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
    let output_name = if argv.len() == 2 {
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
    match build::build_prog(path, &output_name) {
        Ok(()) => exit(0),
        Err(s) => {
            eprintln!("{}", s);
            exit(1)
        }
    }
}

fn deptree(argv: &[String]) -> i32 {
    if argv.len() != 1 {
        eprintln!("usage: deptree <source-path>");
        return 1;
    }
    let path = &argv[0];
    let m = parser::get_module(&basename(path), &path).unwrap();
    let tree = build_tree(&m);
    let r = render_tree(&tree);
    println!("{}", r.join("\n"));
    return 0;
}

struct DepNode {
    name: String,
    deps: Vec<DepNode>,
}

fn build_tree(m: &nodes::Module) -> DepNode {
    let imports = build::module_imports(m);
    let mut deps = vec![];
    for import in imports {
        let m = parser::get_module(&import.path, &m.source_path).unwrap();
        deps.push(build_tree(&m));
    }
    return DepNode {
        name: m.id.clone(),
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

fn exports(argv: &[String]) -> i32 {
    for path in argv {
        let m = parser::get_module(&basename(path), &path).unwrap();
        println!("mod {}", m.id);
        let exports = checkers::get_exports(&m);
        if !exports.consts.is_empty() {
            println!("constants");
            for c in exports.consts {
                println!("\t{}", c.id);
            }
        }
        if !exports.types.is_empty() {
            println!("types");
            for c in exports.types {
                println!("\t{}", c.form.alias);
            }
        }
        if !exports.fns.is_empty() {
            let mut type_width = 0;
            let mut types: Vec<String> = Vec::new();
            let mut forms: Vec<String> = Vec::new();

            for c in exports.fns {
                let mut params = String::from("(");
                for (i, parameter) in c.parameters.list.iter().enumerate() {
                    if i > 0 {
                        params += ", ";
                    }
                    params += &format::format_type(&parameter.type_name);
                    params += " ";
                    for form in &parameter.forms {
                        params += &format::format_form(&form);
                    }
                }
                if c.parameters.variadic {
                    params += ", ...";
                }
                params += ")";

                let t = format::format_type(&c.type_name);
                if t.len() > type_width {
                    type_width = t.len();
                }
                types.push(String::from(t));
                forms.push(format!("{} {}", &format::format_form(&c.form), &params));
            }
            println!("functions");
            for (i, t) in types.iter().enumerate() {
                print!("\t{}", t);
                for _ in 0..(type_width - t.len() + 2) {
                    print!(" ");
                }
                if !forms[i].starts_with("*") {
                    print!(" ");
                }
                print!("{}\n", forms[i]);
            }
        }
    }
    return 0;
}
