mod buf;
mod build;
mod checkers;
mod format;
mod lexer;
mod main_deptree;
mod main_exports;
mod main_genc;
mod main_test;
mod nodes;
mod parser;
#[cfg(test)]
mod rename;
mod translator;
use std::env;

use std::path::Path;
use std::process::exit;
use std::string::String;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() == 1 {
        usage();
        exit(1);
    }
    match args[1].as_str() {
        "build" => {
            exit(build(&args[2..]));
        }
        "genc" => {
            exit(main_genc::run(&args[2..]));
        }
        "deptree" => {
            exit(main_deptree::run(&args[2..]));
        }
        "exports" => {
            exit(main_exports::run(&args[2..]));
        }
        "test" => {
            exit(main_test::run(&args[2..]));
        }
        _ => {
            usage();
            exit(1);
        }
    }
}

fn usage() {
    eprintln!("usage: che build | deptree | exports | test");
}

fn build(argv: &[String]) -> i32 {
    if argv.len() < 1 || argv.len() > 2 {
        eprintln!("usage: build <source-path> [<output-path>]");
        return 1;
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
        Ok(()) => {
            return 0;
        }
        Err(s) => {
            eprintln!("{}", s);
            return 1;
        }
    }
}

#[cfg(test)]
mod test {
    use crate::checkers;
    use crate::format;
    use crate::parser;
    use crate::rename;
    use crate::translator;
    use std::fs;

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
}
