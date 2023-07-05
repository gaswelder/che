mod buf;
mod build;
mod c;
mod checkers;
mod exports;
mod format;
mod format_che;
mod lexer;
mod main_build;
mod main_deptree;
mod main_exports;
mod main_genc;
mod main_test;
mod nodes;
mod nodes_c;
mod parser;
mod preparser;
mod rename;
mod resolve;
mod translator;
use std::env;
use std::process::exit;
use std::string::String;

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
        "test" => main_test::run(&args[2..]),
        _ => usage(),
    });
}

fn usage() -> i32 {
    eprintln!("usage: che <build|deptree|exports|genc|test>");
    return 1;
}
