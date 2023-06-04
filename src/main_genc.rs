use crate::build;
use std::fs;

pub fn run(args: &[String]) -> i32 {
    let modpath = &args[0];
    let dirpath = &args[1];
    if fs::metadata(&dirpath).is_err() {
        fs::create_dir(&dirpath).unwrap();
    }
    let c_modules = build::translate(&modpath).unwrap();
    build::write_c99(&c_modules, &dirpath).unwrap();
    return 0;
}
