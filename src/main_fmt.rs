use crate::build::parse_project;
use crate::format_che;
use std::string::String;

pub fn run(args: &[String]) -> i32 {
    for arg in args {
        let proj = parse_project(arg).unwrap();
        for (i, x) in proj.source_modules_info.iter().enumerate() {
            if &x.loc.path == arg {
                let m = &proj.source_modules[i];
                print!("{}", &format_che::fmt_mod(&m));
            }
        }
    }
    return 0;
}
