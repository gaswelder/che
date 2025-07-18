use crate::c;
use crate::errors::BuildError;
use crate::format_c;
use crate::lexer;
use crate::nodes::Module;
use crate::parser;
use crate::preparser;
use crate::preparser::ModuleInfo;
use crate::resolve;
use crate::resolve::basename;
use crate::translator;
use md5;
use std::collections::HashMap;
use std::collections::HashSet;
use std::env;
use std::fs;
use std::io::BufRead;
use std::process::{Command, Stdio};

#[derive(Debug)]
pub struct Project {
    pub modheads: Vec<ModuleInfo>,
    pub modules: Vec<Module>,
    pub cmodules: Vec<c::CModule>,
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

    let proj = parse_project(source_path)?;

    // translate(&mut proj).map_err(|e| vec![e])?;
    let pathsmap = write_c99(&proj, &tmp_dir_path).unwrap();

    // Determine the list of OS libraries to link. "m" is the library for code
    // in included <math.h>. For simplicity both the header and the library are
    // always included.
    // Other libraries are simply taked from the #link hints in the source code.
    let mut link: Vec<String> = vec!["m".to_string()];
    for c in proj.cmodules {
        for l in &c.link {
            if link.iter().position(|x| x == l).is_none() {
                link.push(l.clone());
            }
        }
    }

    let mut cmd = Command::new("c99");
    let args: &[&str] = if env::consts::OS == "macos" {
        &["-g"]
    } else {
        &[
            "-Wall",
            "-Wextra",
            "-Werror",
            "-pedantic",
            "-pedantic-errors",
            "-fmax-errors=1",
            "-Wno-parentheses",
            "-g",
        ]
    };
    cmd.args(args);
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

fn preparse<'a>(
    cache: &'a mut HashMap<String, ModuleInfo>,
    path: &str,
) -> Result<&'a ModuleInfo, String> {
    if !cache.contains_key(path) {
        cache.insert(path.to_string(), preparser::preparse(path)?);
    }
    Ok(cache.get(path).unwrap())
}

fn load_tree(
    mut cache: &mut HashMap<String, ModuleInfo>,
    path: &str,
    mut crumbs: Vec<String>,
) -> Result<Vec<String>, String> {
    if crumbs.contains(&path.to_string()) {
        return Err(format!("import loop: {} -> {}", crumbs.join(" -> "), path));
    }
    crumbs.push(path.to_string());
    let mut result = vec![path.to_string()];
    let head = preparse(&mut cache, path)?;
    let ii = head.imports.clone();
    for imp in ii {
        let mut r = load_tree(&mut cache, &imp.path, crumbs.clone())?;
        result.append(&mut r);
    }
    Ok(result)
}

fn uniq(ss: Vec<String>) -> Vec<String> {
    let mut seen = HashSet::new();
    let mut result = Vec::new();
    for p in ss {
        if seen.contains(&p) {
            continue;
        }
        seen.insert(p.clone());
        result.push(p);
    }
    result
}

fn parse_mods(modheads: &Vec<ModuleInfo>) -> Result<Vec<Module>, Vec<BuildError>> {
    let mut modules = Vec::new();
    for m in modheads {
        let ctx = parser::ParseCtx {
            thismod: m.clone(),
            allmods: modheads.clone(),
        };
        let path = &m.filepath;
        let mut l = lexer::for_file(&path).unwrap();
        modules.push(parser::parse_module(&mut l, &ctx).map_err(|errors| {
            let mut ee = Vec::new();
            for err in errors {
                ee.push(BuildError {
                    message: err.message,
                    pos: err.pos.fmt(),
                    path: path.clone(),
                })
            }
            ee
        })?);
    }
    Ok(modules)
}

// Parses the full project starting with the file at mainpath
// and including and parsing all its dependencies.
pub fn parse_project(mainpath: &String) -> Result<Project, Vec<BuildError>> {
    let mut cache = HashMap::new();
    let mut paths = load_tree(&mut cache, mainpath, vec![]).map_err(|err| {
        vec![BuildError {
            message: err,
            path: String::new(),
            pos: String::new(),
        }]
    })?;

    paths.reverse();
    paths = uniq(paths);

    let mut modheads = Vec::new();
    for p in paths {
        modheads.push(cache.get(&p).unwrap().clone())
    }

    // Every modhead already has a unique key based on path, but we'll give
    // them nicer ones here to make outputs easier for debugging.
    for (i, m) in modheads.iter_mut().enumerate() {
        m.uniqid = format!("mod{}_{}", i, basename(&m.filepath).replace(".c", ""));
    }

    let modules = parse_mods(&modheads)?;
    let cmodules = translate_mods(modules.clone(), &modheads).map_err(|e| vec![e])?;
    Ok(Project {
        modheads,
        modules,
        cmodules,
    })
}

fn translate_mods(
    mods: Vec<Module>,
    modmetas: &Vec<ModuleInfo>,
) -> Result<Vec<c::CModule>, BuildError> {
    let n = mods.len();

    let mut cmods = Vec::new();
    for i in 0..n {
        let ctx = translator::TrParams {
            cmods: cmods.clone(),
            all_mod_heads: modmetas.clone(),
            this_mod_head: modmetas[i].clone(),
            mods: mods.clone(),
        };
        cmods.push(translator::translate(&mods[i], &ctx)?);
    }
    Ok(cmods)
}

pub fn write_c99(work: &Project, dirpath: &String) -> Result<Vec<PathId>, String> {
    // Write the generated C source files in the temp directory and build the
    // mapping of the generated C file path to the original source file path,
    // that will be used to trace C compiler's errors at least to the original
    // files.
    let mut paths: Vec<PathId> = Vec::new();
    for (i, cm) in work.cmodules.iter().enumerate() {
        let src = format_c::format_module(&cm);
        let path = format!(
            "{}/{:x}.c",
            dirpath,
            md5::compute(&work.modheads[i].filepath)
        );
        paths.push(PathId {
            path: String::from(&path),
            id: String::from(&work.modheads[i].filepath),
        });
        fs::write(&path, &src).unwrap();
    }
    return Ok(paths);
}
