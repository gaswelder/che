use crate::c;
use crate::errors::BuildError;
use crate::format_c;
use crate::nodes;
use crate::parser;
use crate::preparser;
use crate::preparser::ModuleInfo;
use crate::resolve;
use crate::resolve::resolve_import;
use crate::resolve::ModuleRef;
use crate::translator;
use md5;
use std::collections::HashMap;
use std::collections::HashSet;
use std::env;
use std::fs;
use std::io::BufRead;
use std::process::{Command, Stdio};

pub struct Project {
    pub source_modules: Vec<nodes::Module>,
    pub source_modules_info: Vec<ModuleInfo>,
    pub translated_modules: Vec<c::Module>,
}

pub struct PathId {
    path: String,
    id: String,
}

pub fn build_prog(source_path: &str, output_name: &str) -> Result<(), Vec<BuildError>> {
    // Decide where we'll stash all generated C code.
    let tmp_dir_path = format!("{}/tmp", resolve::homepath());
    if fs::metadata(&tmp_dir_path).is_err() {
        fs::create_dir(&tmp_dir_path).unwrap();
    }

    let proj = parse_project(source_path)?;
    let pathsmap = write_c99(&proj, &tmp_dir_path).unwrap();

    let mut link = vec!["m".to_string()]; // always link math.h for simplicity.

    // Add other libraries as the #link hints say.
    for c in proj.translated_modules {
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

fn load_tree(
    mut cache: &mut HashMap<String, ModuleInfo>,
    loc: &ModuleRef,
    mut crumbs: Vec<String>,
) -> Result<Vec<String>, String> {
    if crumbs.contains(&loc.path) {
        return Err(format!(
            "import loop: {} -> {}",
            crumbs.join(" -> "),
            &loc.path
        ));
    }
    crumbs.push(loc.path.clone());

    let mut result = vec![loc.path.clone()];
    if !cache.contains_key(&loc.path) {
        cache.insert(loc.path.clone(), preparser::preparse(loc)?);
    }
    let head = cache.get(&loc.path).unwrap();
    let ii = head.imports.clone();
    for imp in ii {
        let mut r = load_tree(&mut cache, &imp, crumbs.clone())?;
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

// Parses the full project starting with the file at mainpath
// and including and parsing all its dependencies.
pub fn parse_project(mainpath: &str) -> Result<Project, Vec<BuildError>> {
    let mut cache = HashMap::new();

    let main_loc = resolve_import(".", mainpath).unwrap();
    let mut paths = load_tree(&mut cache, &main_loc, vec![]).map_err(|err| {
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
        m.uniqid = format!(
            "mod{}_{}",
            i,
            resolve::basename(&m.loc.path)
                .replace(".c", "")
                .replace(".unix", "")
        );
    }

    let mut modules = Vec::new();
    for m in &modheads {
        modules.push(parser::parse_module(&modheads, &m)?);
    }
    let cmodules = translator::translate_mods(modules.clone(), &modheads).map_err(|e| vec![e])?;
    Ok(Project {
        source_modules_info: modheads,
        source_modules: modules,
        translated_modules: cmodules,
    })
}

// Write the generated C source files in the temp directory and build the
// mapping of the generated C file path to the original source file path,
// that will be used to trace C compiler's errors at least to the original
// files.
pub fn write_c99(work: &Project, dirpath: &str) -> Result<Vec<PathId>, String> {
    let mut paths: Vec<PathId> = Vec::new();
    for (i, cm) in work.translated_modules.iter().enumerate() {
        let source_path = format!(
            "{}/{:x}.c",
            dirpath,
            md5::compute(&work.source_modules_info[i].loc.path)
        );
        paths.push(PathId {
            path: String::from(&source_path),
            id: String::from(&work.source_modules_info[i].loc.path),
        });

        fs::write(&source_path, format_c::format_module(&cm)).unwrap();
    }
    return Ok(paths);
}
