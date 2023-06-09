use crate::build;
use crate::checkers;
use crate::checkers::Exports;
use crate::format_che;
use std::path::Path;

fn basename(path: &String) -> String {
    return String::from(Path::new(path).file_name().unwrap().to_str().unwrap());
}

pub fn run(argv: &[String]) -> i32 {
    for path in argv {
        // let nodes = build::parse(path).unwrap();
        // let m = nodes[0].m.as_ref().unwrap();
        // println!("mod {}", nodes[0].ctx.path);
        // let exports = checkers::get_exports(&m);
    }
    return 0;
}

fn print_exports(exports: Exports) {
    if !exports.consts.is_empty() {
        println!("constants");
        for c in exports.consts {
            println!("\t{}", c.id);
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
            for (i, parameter) in c.parameters.list.iter().enumerate() {
                if i > 0 {
                    params += ", ";
                }
                params += &format_che::format_type(&parameter.type_name);
                params += " ";
                for form in &parameter.forms {
                    params += &format_che::format_form(&form);
                }
            }
            if c.parameters.variadic {
                params += ", ...";
            }
            params += ")";

            let t = format_che::format_type(&c.type_name);
            if t.len() > type_width {
                type_width = t.len();
            }
            types.push(String::from(t));
            forms.push(format!("{} {}", &format_che::format_form(&c.form), &params));
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
