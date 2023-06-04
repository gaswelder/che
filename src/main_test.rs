use crate::build;
use std::fs;
use std::process::Command;
use std::str;
use std::string::String;

struct Stats {
    total: i32,
    fails: i32,
}

pub fn run(args: &[String]) -> i32 {
    let mut s = Stats { total: 0, fails: 0 };
    for arg in args {
        match run_arg(arg) {
            Ok(stats1) => {
                s.total += stats1.total;
                s.fails += stats1.fails;
            }
            Err(err) => {
                println!("{}", err);
            }
        }
    }
    println!("fails: {}, total: {}", s.fails, s.total);
    return s.fails;
}

fn run_arg(arg: &String) -> Result<Stats, String> {
    let tests = find_tests(arg)?;
    let mut stats = Stats {
        total: tests.len() as i32,
        fails: 0,
    };
    for path in tests {
        build::build_prog(&path, &String::from("test.out"))?;
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
            stats.fails += 1;
            println!("FAIL {}", path);
            println!("stdout:\n{}", outstr);
        }
        if errstr != "" {
            println!("stderr:\n{}", errstr);
        }
    }
    println!("{} tests", stats.total);
    return Ok(stats);
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
