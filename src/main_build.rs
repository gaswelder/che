use crate::build;
use std::path::Path;

pub fn run(argv: &[String]) -> i32 {
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
