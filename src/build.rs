use crate::check_identifiers;
use crate::checkers;
use crate::exports::get_exports;
use crate::format;
use crate::lexer;
use crate::nodes::Module;
use crate::nodes_c::CModule;
use crate::nodes_c::CModuleObject;
use crate::parser;
use crate::preparser;
use crate::preparser::Import;
use crate::rename;
use crate::resolve;
use crate::translator;
use md5;
use std::collections::HashMap;
use std::fs;
use std::io::BufRead;
use std::process::{Command, Stdio};

#[derive(Clone, Debug)]
pub struct Ctx {
    pub path: String,
    pub typenames: Vec<String>,
    pub deps: Vec<Dep>,
}

impl Ctx {
    pub fn has_ns(&self, ns: &String) -> bool {
        for dep in &self.deps {
            if dep.ns == *ns {
                return true;
            }
        }
        return false;
    }
}

#[derive(Clone, Debug)]
pub struct Dep {
    pub ns: String,
    pub typenames: Vec<String>,
    pub path: String,
}

#[derive(Debug)]
pub struct Build {
    pub paths: Vec<String>,
    pub typenames: Vec<Vec<String>>,
    pub imports: Vec<Vec<Import>>,
    pub ctx: Vec<Ctx>,
    pub m: Vec<Module>,
    pub c: Vec<CModule>,
}

pub struct PathId {
    path: String,
    id: String,
}

pub fn build_prog(source_path: &String, output_name: &String) -> Result<(), String> {
    // Decide where we'll stash all generated C code.
    let tmp_dir_path = format!("{}/tmp", resolve::homepath());
    if fs::metadata(&tmp_dir_path).is_err() {
        fs::create_dir(&tmp_dir_path).unwrap();
    }

    let mut work = parse(source_path)?;
    translate(&mut work)?;
    let pathsmap = write_c99(&work, &tmp_dir_path).unwrap();

    // Determine the list of OS libraries to link. "m" is the library for code
    // in included <math.h>. For simplicity both the header and the library are
    // always included.
    // Other libraries are simply taked from the #link hints in the source code.
    let mut link: Vec<String> = vec!["m".to_string()];
    for c in work.c {
        for l in &c.link {
            if link.iter().position(|x| x == l).is_none() {
                link.push(l.clone());
            }
        }
    }

    let mut cmd = Command::new("c99");
    cmd.args(&[
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic",
        "-pedantic-errors",
        "-fmax-errors=1",
        "-Wno-parentheses",
        "-g",
    ]);
    for p in &pathsmap {
        cmd.arg(p.path.clone());
    }
    cmd.args(&["-o", &output_name]);
    for l in link {
        cmd.args(&["-l", &l.trim()]);
    }
    cmd.stderr(Stdio::piped());
    let mut proc = cmd.spawn().unwrap();
    let out = proc.stderr.as_mut().unwrap();
    let reader = std::io::BufReader::new(out);
    for liner in reader.lines() {
        let mut line = liner.unwrap();
        for p in &pathsmap {
            line = line.replace(&p.path, &format!("{} -> {}", &p.id, &p.path));
        }
        println!("{}", &line);
    }
    let r = proc.wait().unwrap();
    if r.success() {
        return Ok(());
    }
    return Err(String::from("build failed"));
}

pub fn parse(mainpath: &String) -> Result<Build, String> {
    let mut work = Build {
        paths: vec![mainpath.clone()],
        typenames: Vec::new(),
        imports: Vec::new(),
        ctx: Vec::new(),
        m: Vec::new(),
        c: Vec::new(),
    };

    let kek = preparser::preparse(mainpath)?;
    work.typenames.push(kek.typenames);
    work.imports.push(kek.imports);

    loop {
        // find an unresolved import
        let mut missing: Option<&Import> = None;
        for imps in &work.imports {
            for imp in imps {
                if !work.paths.contains(&imp.path) {
                    missing = Some(imp);
                    break;
                }
            }
            if missing.is_some() {
                break;
            }
        }
        if missing.is_none() {
            break;
        }
        let imp = missing.unwrap();
        work.paths.push(imp.path.clone());
        let kek = preparser::preparse(&imp.path)?;
        work.typenames.push(kek.typenames.clone());
        work.imports.push(kek.imports.clone());
    }

    work.paths.reverse();
    work.typenames.reverse();
    work.imports.reverse();

    for (i, path) in work.paths.iter().enumerate() {
        let mut deps: Vec<Dep> = Vec::new();
        for imp in &work.imports[i] {
            let pos = work.paths.iter().position(|x| *x == imp.path).unwrap();
            deps.push(Dep {
                ns: imp.ns.clone(),
                typenames: work.typenames[pos].clone(),
                path: imp.path.clone(),
            });
        }
        work.ctx.push(Ctx {
            typenames: work.typenames[i].clone(),
            path: path.clone(),
            deps,
        });
    }

    // Parse each item.
    for (i, path) in work.paths.iter().enumerate() {
        let mut l = lexer::for_file(&path)?;
        let ctx = &work.ctx[i];
        work.m.push(parser::parse_module(&mut l, ctx)?);
    }

    //
    // Dome some checks.
    //

    // path -> exports map
    let mut exports_by_path = HashMap::new();
    for (i, m) in work.m.iter().enumerate() {
        let path = &work.paths[i];
        exports_by_path.insert(path, get_exports(m));
    }

    for (i, m) in work.m.iter().enumerate() {
        for imp in &work.imports[i] {
            let pos = work.paths.iter().position(|x| *x == imp.path).unwrap();
            let depm = &work.m[pos];
            if !checkers::depused(&m, &depm, &imp.ns) {
                return Err(format!(
                    "{}: imported {} is not used",
                    work.paths[i], &imp.ns
                ));
            }
        }

        // import alias -> exports map for this module
        let mut exports = HashMap::new();
        for imp in &work.imports[i] {
            exports.insert(imp.ns.clone(), exports_by_path.get(&imp.path).unwrap());
        }
        let errors = check_identifiers::run(m, &exports);
        if errors.len() > 0 {
            let mut lines: Vec<String> = Vec::new();
            for err in errors {
                lines.push(format!("{} at {}:{}", err.message, work.paths[i], err.pos))
            }
            return Err(lines.join("\n"));
        }
    }
    return Ok(work);
}

pub fn translate(work: &mut Build) -> Result<(), String> {
    // Globalize all modules
    for (i, m) in work.m.iter_mut().enumerate() {
        let path = &work.paths[i];
        rename::globalize_module(m, &translator::module_gid(path));
    }
    println!("globaized");

    // Translate each node.
    for (i, m) in work.m.iter().enumerate() {
        let mut cnodes: Vec<CModuleObject> = Vec::new();
        // Include all standard C library everywhere.
        let std = vec![
            "assert", "ctype", "errno", "limits", "math", "stdarg", "stdbool", "stddef", "stdint",
            "stdio", "stdlib", "string", "time", "setjmp",
        ];
        for n in std {
            cnodes.push(CModuleObject::Include(format!("<{}.h>", n)));
        }
        // Include custom utils
        cnodes.push(CModuleObject::Macro {
            name: "define".to_string(),
            value: "nelem(x) (sizeof (x)/sizeof (x)[0])".to_string(),
        });
        for imp in &work.imports[i] {
            let pos = work.paths.iter().position(|x| *x == imp.path).unwrap();
            let c = &work.c[pos];
            let syn = translator::get_module_synopsis(&c);
            for obj in syn {
                cnodes.push(obj);
            }
        }
        let ctx = &work.ctx[i];
        let mut c = translator::translate(m, ctx);
        for e in c.elements {
            cnodes.push(e);
        }
        c.elements = cnodes;
        work.c.push(c);
    }

    return Ok(());
}

pub fn write_c99(work: &Build, dirpath: &String) -> Result<Vec<PathId>, String> {
    // Write the generated C source files in the temp directory and build the
    // mapping of the generated C file path to the original source file path,
    // that will be used to trace C compiler's errors at least to the original
    // files.
    let mut paths: Vec<PathId> = Vec::new();
    for (i, cm) in work.c.iter().enumerate() {
        let src = format::format_module(&cm);
        let path = format!("{}/{:x}.c", dirpath, md5::compute(&work.paths[i]));
        paths.push(PathId {
            path: String::from(&path),
            id: String::from(&work.paths[i]),
        });
        fs::write(&path, &src).unwrap();
    }
    return Ok(paths);
}
