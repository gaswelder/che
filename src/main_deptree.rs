use crate::build;
use std::path::Path;
use std::str;
use std::string::String;

fn basename(path: &String) -> String {
    return String::from(Path::new(path).file_name().unwrap().to_str().unwrap());
}

struct DepNode {
    name: String,
    deps: Vec<DepNode>,
}

pub fn run(argv: &[String]) -> i32 {
    if argv.len() != 1 {
        eprintln!("usage: deptree <source-path>");
        return 1;
    }
    let path = &argv[0];
    // let tree = build::gettree(path).unwrap();
    // let r = render_tree(&tree);
    // println!("{}", r.join("\n"));
    return 0;
}

// fn render_tree(tree: &TreeNode) -> Vec<String> {
//     let mut lines: Vec<String> = vec![tree.ctx.path.clone()];
//     let n = tree.dependencies.len();
//     for (i, dep) in tree.dependencies.iter().enumerate() {
//         let r = render_tree(&dep.node);
//         if i == n - 1 {
//             lines.append(&mut indent_tree(r, " └", "  "));
//         } else {
//             lines.append(&mut indent_tree(r, " ├", " │"));
//         }
//     }
//     return lines;
// }

// fn indent_tree(lines: Vec<String>, first: &str, cont: &str) -> Vec<String> {
//     let mut result: Vec<String> = Vec::new();
//     for (i, line) in lines.iter().enumerate() {
//         if i == 0 {
//             result.push(format!("{}{}", first, line))
//         } else {
//             result.push(format!("{}{}", cont, line));
//         }
//     }
//     return result;
// }
