use crate::c;
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
use std::collections::HashSet;
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

pub fn build_prog(source_path: &String, output_name: &String) -> Result<(), Vec<BuildError>> {
    // Decide where we'll stash all generated C code.
    let tmp_dir_path = format!("{}/tmp", resolve::homepath());
    if fs::metadata(&tmp_dir_path).is_err() {
        fs::create_dir(&tmp_dir_path).unwrap();
    }

    let mut work = parse(source_path)?;
    translate(&mut work);
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
    return Err(vec![BuildError {
        message: String::from("build failed"),
        path: String::from(""),
        pos: String::from(""),
    }]);
}

pub struct BuildError {
    pub path: String,
    pub pos: String,
    pub message: String,
}

pub fn parse(mainpath: &String) -> Result<Build, Vec<BuildError>> {
    let mut work = Build {
        paths: Vec::new(),
        typenames: Vec::new(),
        imports: Vec::new(),
        ctx: Vec::new(),
        m: Vec::new(),
        c: Vec::new(),
    };

    // Load pre-parsed modules in their dependency order.
    let mut preps = HashMap::new();
    let mut i = 0;
    let mut deppath: Vec<String> = vec![mainpath.clone()];
    while i < deppath.len() {
        let p = &deppath[i];
        if !preps.contains_key(p) {
            let prep = preparser::preparse(p).map_err(|message| {
                vec![BuildError {
                    message,
                    pos: "".to_string(),
                    path: p.clone(),
                }]
            })?;
            preps.insert(p.clone(), prep);
        }
        let prep = preps.get(p).unwrap();
        for imp in &prep.imports {
            deppath.push(imp.path.clone());
        }
        i += 1;
    }
    deppath.reverse();

    // Load the resolved dependencies into the build in reverse order,
    // skipping duplicates on the way. This basically achieves topological
    // ordering of the dependencies.
    for p in deppath {
        if work.paths.contains(&p) {
            continue;
        }
        let prep = preps.get(&p).unwrap();
        work.paths.push(p);
        work.typenames.push(prep.typenames.clone());
        work.imports.push(prep.imports.clone());
    }

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
        // Add a fake dep for OS calls.
        deps.push(Dep {
            ns: String::from("OS"),
            typenames: Vec::new(),
            path: String::from(""),
        });
        work.ctx.push(Ctx {
            typenames: work.typenames[i].clone(),
            path: path.clone(),
            deps,
        });
    }

    // Parse each item.
    for (i, path) in work.paths.iter().enumerate() {
        let mut l = lexer::for_file(&path).unwrap();
        let ctx = &work.ctx[i];
        let m = parser::parse_module(&mut l, ctx).map_err(|err| {
            vec![BuildError {
                message: err.message,
                pos: err.pos,
                path: path.clone(),
            }]
        })?;
        work.m.push(m);
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

    let mut errors: Vec<BuildError> = Vec::new();
    for (i, m) in work.m.iter().enumerate() {
        // Make a map namespace->exports for this module's imports.
        // The checkers will do lookups based on namespace part on identifiers.
        let mut dependencies = HashMap::new();
        for imp in &work.imports[i] {
            dependencies.insert(imp.ns.clone(), exports_by_path.get(&imp.path).unwrap());
        }
        for err in checkers::run(m, &dependencies) {
            errors.push(BuildError {
                message: err.message,
                path: work.paths[i].clone(),
                pos: err.pos,
            });
        }
    }
    if errors.len() > 0 {
        return Err(errors);
    }
    return Ok(work);
}

pub fn translate(work: &mut Build) {
    // Globalize all modules
    for (i, m) in work.m.iter_mut().enumerate() {
        let path = &work.paths[i];
        rename::prefix_module(m, &translator::module_gid(path));
    }

    // Translate each node.
    for (i, m) in work.m.iter().enumerate() {
        let mut cnodes: Vec<CModuleObject> = Vec::new();

        // A hack to make os libs build on Linux.
        cnodes.push(CModuleObject::Macro {
            name: "define".to_string(),
            value: "_XOPEN_SOURCE 700".to_string(),
        });

        // Include all standard C library.
        for n in c::CLIBS {
            cnodes.push(CModuleObject::Include(format!("<{}.h>", n)));
        }
        // Include custom utils
        cnodes.push(CModuleObject::Macro {
            name: "define".to_string(),
            value: "nelem(x) (sizeof (x)/sizeof (x)[0])".to_string(),
        });

        let node_imports = &work.imports[i];
        let mut present = HashSet::new();
        for imp in node_imports {
            let pos = work.paths.iter().position(|x| *x == imp.path).unwrap();
            let cmodule = &work.c[pos];
            for obj in translator::get_module_synopsis(&cmodule) {
                // This naive synopsis generation produces duplicates when
                // a module encounters another module in dependencies more than
                // once.
                let id = match &obj {
                    CModuleObject::EnumDefinition {
                        members,
                        is_hidden: _,
                    } => {
                        let mut names: Vec<String> = Vec::new();
                        for m in members {
                            names.push(m.id.clone());
                        }
                        format!("enum-{}", names.join(","))
                    }
                    CModuleObject::Typedef {
                        is_pub: _,
                        type_name: _,
                        form,
                    } => form.alias.clone(),
                    CModuleObject::StructDefinition {
                        name,
                        fields: _,
                        is_pub: _,
                    } => name.clone(),
                    CModuleObject::Include(x) => x.clone(),
                    CModuleObject::Macro { name: _, value } => value.clone(),
                    CModuleObject::StructForwardDeclaration(x) => x.clone(),
                    CModuleObject::ModuleVariable {
                        type_name: _,
                        form: _,
                        value: _,
                    } => String::new(), // We shouldn't see module variables here, they are unexportable.
                    CModuleObject::FunctionForwardDeclaration {
                        is_static: _,
                        type_name: _,
                        form,
                        parameters: _,
                    } => form.name.clone(),
                    CModuleObject::FunctionDefinition {
                        is_static: _,
                        type_name: _,
                        form: _,
                        parameters: _,
                        body: _,
                    } => String::new(), // Shouldn't see functions here.
                };
                if present.contains(&id) {
                    continue;
                }
                present.insert(id);
                cnodes.push(obj)
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
