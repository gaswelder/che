use crate::build;
use std::fs;

pub fn run(args: &[String]) -> i32 {
    let modpath = &args[0];
    let dirpath = &args[1];
    if fs::metadata(&dirpath).is_err() {
        fs::create_dir(&dirpath).unwrap();
    }
    match genc(&modpath, &dirpath) {
        Ok(()) => {
            return 0;
        }
        Err(s) => {
            eprintln!("{}", s);
            return 1;
        }
    }
}

fn genc(mainpath: &String, dirpath: &String) -> Result<(), String> {
    let mut work = build::parse(mainpath)?;
    build::translate(&mut work)?;
    build::write_c99(&work, dirpath)?;
    return Ok(());
}
