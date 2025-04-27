use crate::build::{self, Project};
use std::string::String;

pub fn run(argv: &[String]) -> i32 {
    if argv.len() != 1 {
        eprintln!("usage: deptree <source-path>");
        return 1;
    }
    let path = &argv[0];
    match build::parse_project(path) {
        Ok(build) => {
            let pos = build.modheads.len();
            let mut p = Printer {
                indent: 0,
                first_line: true,
            };
            render_tree(&mut p, &build, pos - 1);
            return 0;
        }
        Err(errors) => {
            for err in errors {
                eprintln!("{}:{}: {}", err.path, err.pos, err.message);
            }
            return 1;
        }
    }
}

struct Printer {
    indent: usize,
    first_line: bool,
}

impl Printer {
    fn writeline(&mut self, s: &String) {
        for _ in 0..self.indent {
            print!("  ");
        }
        if self.indent > 0 && self.first_line {
            print!("{}", if self.first_line { "- " } else { "- " });
        }
        println!("{}", s);
        self.first_line = false;
    }
    fn indent(&mut self) {
        self.indent += 1;
        self.first_line = true;
    }
    fn unindent(&mut self) {
        self.indent -= 1;
        self.first_line = true;
    }
}

fn render_tree(p: &mut Printer, build: &Project, pos: usize) {
    let path = &build.modheads[pos].filepath;
    p.writeline(path);
    p.indent();
    for imp in &build.modheads[pos].imports {
        let deppos = build
            .modheads
            .iter()
            .position(|x| x.filepath == imp.path)
            .unwrap();
        render_tree(p, build, deppos);
    }
    p.unindent();
}
