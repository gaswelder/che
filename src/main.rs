mod buf;
mod build;
mod c;
mod cspec;
mod format_c;
mod format_che;
mod lexer;
mod main_build;
mod main_deptree;
mod main_exports;
mod main_genc;
mod main_install;
mod main_test;
mod node_queries;
mod nodes;
mod parser;
mod preparser;
mod resolve;
mod translator;
mod types;
use std::env;
use std::process::exit;
use std::string::String;
mod errors;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() == 1 {
        usage();
        exit(1);
    }
    exit(match args[1].as_str() {
        "build" => main_build::run(&args[2..]),
        "deptree" => main_deptree::run(&args[2..]),
        "exports" => main_exports::run(&args[2..]),
        "genc" => main_genc::run(&args[2..]),
        "install" => main_install::run(&args[2..]),
        "test" => main_test::run(&args[2..]),
        _ => usage(),
    });
}

fn usage() -> i32 {
    eprintln!("usage: che <build|deptree|exports|genc|install|test>");
    return 1;
}
