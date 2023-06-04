use crate::build;
use std::fs;

pub fn run(args: &[String]) -> i32 {
    let modpath = &args[2];
    let dirpath = &args[3];
    if fs::metadata(&dirpath).is_err() {
        fs::create_dir(&dirpath).unwrap();
    }
    let c_modules = build::translate(&modpath).unwrap();
    build::write_c99(&c_modules, &dirpath).unwrap();
    return 0;
}
