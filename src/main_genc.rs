use crate::build;
use std::fs;

pub fn run(args: &[String]) -> i32 {
    let modpath = &args[0];
    let dirpath = &args[1];
    if fs::metadata(&dirpath).is_err() {
        fs::create_dir(&dirpath).unwrap();
    }
    let work = match build::parse_project(modpath) {
        Ok(work) => work,
        Err(errors) => {
            for err in errors {
                eprintln!("{}:{}: {}", err.path, err.pos, err.message);
            }
            return 1;
        }
    };
    // build::translate(&mut work);
    build::write_c99(&work, dirpath).unwrap();
    return 0;
}
