use crate::build::{self, Build};
use std::string::String;

pub fn run(argv: &[String]) -> i32 {
    if argv.len() != 1 {
        eprintln!("usage: deptree <source-path>");
        return 1;
    }
    let path = &argv[0];
    let build = build::parse(path).unwrap();
    let pos = build.paths.len();
    let mut p = Printer {
        indent: 0,
        first_line: true,
    };
    render_tree(&mut p, &build, pos - 1);
    return 0;
}

struct Printer {
    indent: usize,
    first_line: bool,
}

impl Printer {
    fn writeline(&mut self, s: &String) {
        for _ in 0..self.indent {
            print!(" ");
        }
        if self.indent > 0 && self.first_line {
            print!("{}", if self.first_line { " └" } else { " ├" });
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

fn render_tree(p: &mut Printer, build: &Build, pos: usize) {
    let path = &build.paths[pos];
    p.writeline(path);
    p.indent();
    for imp in &build.imports[pos] {
        let deppos = build.paths.iter().position(|x| *x == imp.path).unwrap();
        render_tree(p, build, deppos);
    }
    p.unindent();
}
