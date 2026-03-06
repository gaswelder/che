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
mod main_run;
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

type SubcommandFn = fn(&[String]) -> i32;

static SUBCOMMANDS: &[(&str, &str, SubcommandFn)] = &[
    (
        "build",
        "Build executable from a source file",
        main_build::run,
    ),
    ("run", "Build from a source file and run", main_run::run),
    ("deptree", "Print dependency tree", main_deptree::run),
    ("exports", "Print module exports", main_exports::run),
    ("genc", "Generate C code", main_genc::run),
    (
        "install",
        "Build and install executable in $CHELANG_HOME/bin",
        main_install::run,
    ),
    ("test", "Run tests", main_test::run),
];

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() == 1 {
        usage();
        exit(1);
    }
    let cmd = args[1].as_str();
    match SUBCOMMANDS.iter().find(|(name, _, _)| *name == cmd) {
        Some((_, _, func)) => exit(func(&args[2..])),
        None => usage(),
    };
}

fn usage() {
    eprintln!("Usage: che <command>\n");
    eprintln!("Commands:");
    for (name, desc, _) in SUBCOMMANDS {
        eprintln!("\t{}\t{}", name, desc);
    }
}
