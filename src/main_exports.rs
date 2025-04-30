use crate::build;
use crate::format_che;
use crate::nodes::Exports;

pub fn run(argv: &[String]) -> i32 {
    for path in argv {
        match build::parse_project(path) {
            Ok(build) => {
                let pos = build.modules.len() - 1;
                let m = &build.modules[pos];
                let path = &build.modheads[pos].filepath;
                println!("mod {}", path);
                let exports = m.exports.clone();
                print_exports(exports);
            }
            Err(errors) => {
                for err in errors {
                    eprintln!("{}:{}: {}", err.path, err.pos, err.message);
                }
                return 1;
            }
        }
    }
    return 0;
}

fn print_exports(exports: Exports) {
    if !exports.consts.is_empty() {
        println!("constants");
        for c in exports.consts {
            println!("\t{}", c.name);
        }
    }
    if !exports.types.is_empty() {
        println!("types");
        for c in exports.types {
            println!("\t{}", c);
        }
    }
    if !exports.fns.is_empty() {
        let mut type_width = 0;
        let mut types: Vec<String> = Vec::new();
        let mut forms: Vec<String> = Vec::new();

        for c in exports.fns {
            let mut params = String::from("(");
            for (i, parameter) in c.params.list.iter().enumerate() {
                if i > 0 {
                    params += ", ";
                }
                params += &format_che::fmt_typename(&parameter.typename);
                params += " ";
                for form in &parameter.forms {
                    params += &format_che::fmt_form(&form);
                }
            }
            if c.params.variadic {
                params += ", ...";
            }
            params += ")";

            let t = format_che::fmt_typename(&c.typename);
            if t.len() > type_width {
                type_width = t.len();
            }
            types.push(String::from(t));
            forms.push(format!("{} {}", &format_che::fmt_form(&c.form), &params));
        }
        println!("functions");
        for (i, t) in types.iter().enumerate() {
            print!("\t{}", t);
            for _ in 0..(type_width - t.len() + 2) {
                print!(" ");
            }
            if !forms[i].starts_with("*") {
                print!(" ");
            }
            print!("{}\n", forms[i]);
        }
    }
}
