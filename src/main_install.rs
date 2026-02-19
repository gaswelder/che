use crate::build;
use std::{env, path::Path};

pub fn run(argv: &[String]) -> i32 {
    if argv.len() != 1 {
        eprintln!("usage: install <name>");
        return 1;
    }
    let homedir = env::var("CHELANG_HOME").unwrap();
    let source_path = &argv[0];
    let output_name = Path::new(&source_path)
        .with_extension("")
        .file_name()
        .unwrap()
        .to_str()
        .unwrap()
        .to_string();
    let output_path = format!("{}/bin/{}", homedir, output_name);
    match build::build_prog(&source_path, &output_path) {
        Ok(()) => {
            return 0;
        }
        Err(errors) => {
            let n = errors.len();
            for err in errors {
                eprintln!("{}:{}: {}", err.path, err.pos, err.message);
            }
            eprintln!("{} errors", n);
            return 1;
        }
    }
}
