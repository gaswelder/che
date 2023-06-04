use crate::build;
use std::fs;
use std::process::Command;
use std::str;
use std::string::String;

pub fn run(args: &[String]) -> Result<(), String> {
    let mut fails = 0;
    let mut total = 0;
    for arg in args {
        let tests = find_tests(arg)?;
        for path in tests {
            build::build_prog(&path, &String::from("test.out")).unwrap();
            let output = Command::new("./test.out").output().unwrap();
            let errstr = str::from_utf8(&output.stderr).unwrap();
            let outstr = str::from_utf8(&output.stdout).unwrap().trim_end();
            if output.status.success() {
                if outstr != "" {
                    println!("OK {}: {}", path, outstr);
                } else {
                    println!("OK {}", path);
                }
            } else {
                fails += 1;
                println!("FAIL {}", path);
                println!("stdout:\n{}", outstr);
            }
            if errstr != "" {
                println!("stderr:\n{}", errstr);
            }
            total += 1
        }
        println!("{} tests", total)
    }
    if fails > 1 {
        return Err(format!("{} tests failed", fails));
    }
    if fails == 1 {
        return Err(format!("1 test failed"));
    }
    return Ok(());
}

fn find_tests(dirpath: &String) -> Result<Vec<String>, String> {
    let mut paths: Vec<String> = Vec::new();
    for entry_result in fs::read_dir(dirpath).unwrap() {
        let result = entry_result.unwrap();
        let path = result.path();
        if path.to_str().unwrap().ends_with(".test.c") {
            paths.push(String::from(path.to_str().unwrap()));
        } else if path.is_dir() {
            paths.append(&mut find_tests(&String::from(path.to_str().unwrap())).unwrap())
        }
    }
    return Ok(paths);
}
