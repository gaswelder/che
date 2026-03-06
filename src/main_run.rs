use crate::build;
use std::os::unix::process::CommandExt;
use std::process::Command;
use std::string::String;

pub fn run(args: &[String]) -> i32 {
    if args.len() < 1 {
        eprintln!("usage: che run <source-path> [<args>...]");
        return 1;
    }
    let src = &args[0];
    let run_args = &args[1..];
    match build::build_prog(src, "/tmp/chebuild") {
        Ok(()) => {}
        Err(errors) => {
            for err in errors {
                eprintln!("{}:{}: {}", err.path, err.pos, err.message);
            }
            return 1;
        }
    }
    let mut cmd = Command::new("/tmp/chebuild");
    cmd.args(run_args);
    let err = cmd.exec();
    eprintln!("exec failed: {}", err.to_string());
    return 1;
}
