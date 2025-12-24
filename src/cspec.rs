use std::{collections::HashMap, sync::OnceLock};

use crate::{
    buf::Pos,
    nodes,
    types::{ellipsis, just, justp, todo, Type, TypeOp},
};

#[derive(Debug)]
pub enum Kind {
    TYPE,
    SYM,
}

#[derive(Debug)]
pub struct CSymbol {
    pub kind: Kind,
    pub name: &'static str,
    pub t: Type,
}

static SYMS: OnceLock<HashMap<&str, CSymbol>> = OnceLock::new();

// Returns true if there is built-in type with the given name.
pub fn has_type(name: &str) -> bool {
    find_sym(name).is_some_and(|x| matches!(x.kind, Kind::TYPE))
}

pub fn find_sym(s: &str) -> Option<&CSymbol> {
    getsyms().get(s)
}

fn getsyms() -> &'static HashMap<&'static str, CSymbol> {
    SYMS.get_or_init(buildmap)
}

// A wrapper to declare a built-in type.
fn t(name: &'static str) -> CSymbol {
    CSymbol {
        kind: Kind::TYPE,
        name,
        t: Type {
            base: nodes::NsName {
                ns: "".to_string(),
                name: name.to_string(),
                pos: Pos { col: 0, line: 0 },
            },
            ops: Vec::new(),
        },
    }
}

// A wrapper to declare a built-in constant.
fn c(name: &'static str, t: &Type) -> CSymbol {
    CSymbol {
        kind: Kind::SYM,
        name,
        t: t.clone(),
    }
}

// A wrapper to declare a built-in function.
fn f(name: &'static str, args: Vec<&Type>, rettype: &Type) -> CSymbol {
    let mut argsc = Vec::new();
    for t in args {
        argsc.push(t.clone())
    }
    let mut retc = rettype.clone();
    retc.ops.insert(0, TypeOp::Call(argsc));
    CSymbol {
        kind: Kind::SYM,
        name,
        t: retc,
    }
}

fn buildmap() -> HashMap<&'static str, CSymbol> {
    let bool = &just("bool");
    let charp = &justp("char");
    let chr = &just("char");
    let filep = &justp("FILE");
    let int = &just("int");
    let size = &just("size_t");
    let __todo = &todo();
    let void = &just("void");
    let voidp = &justp("void");
    let dbl = &just("double");
    let flt = &just("float");

    let list = vec![
        // native
        t("char"),
        t("double"),
        t("float"),
        t("int"),
        t("void"),
        // stddef
        c("NULL", voidp),
        t("ptrdiff_t"),
        t("size_t"),
        // stdint
        t("int16_t"),
        t("int32_t"),
        t("int64_t"),
        t("int8_t"),
        t("uint16_t"),
        t("uint32_t"),
        t("uint64_t"),
        t("uint8_t"),
        // stdbool
        t("bool"),
        c("false", bool),
        c("true", bool),
        // limits
        c("INT_MAX", int),
        c("SIZE_MAX", size),
        // stdio
        c("BUFSIZ", size),
        c("SEEK_END", int),
        c("SEEK_SET", int),
        c("SEEK_CUR", int),
        c("EOF", int),
        c("stderr", filep),
        c("stdin", filep),
        c("stdout", filep),
        f("fclose", vec![filep], int),
        f("feof", vec![filep], bool),
        f("ferror", vec![filep], bool),
        f("fflush", vec![filep], int),
        f("fgetc", vec![filep], int),
        f("fgets", vec![voidp, size, filep], charp),
        f("fopen", vec![charp, charp], filep),
        f("fprintf", vec![filep, charp, &ellipsis()], void),
        f("fputc", vec![int, filep], int),
        f("fputs", vec![charp, filep], int),
        f("fread", vec![voidp, size, size, filep], size),
        f("fseek", vec![filep, &just("int32_t"), int], int),
        f("ftell", vec![filep], &just("int32_t")),
        f("fwrite", vec![voidp, size, size, filep], size),
        f("getc", vec![filep], int),
        f("getchar", vec![], int),
        f("printf", vec![charp, &ellipsis()], int),
        f("putc", vec![int, filep], int),
        f("putchar", vec![int], int),
        f("puts", vec![charp], int),
        f("remove", vec![charp], int),        // depr
        f("rename", vec![charp, charp], int), // depr
        f("rewind", vec![filep], void),
        f("scanf", vec![charp, &ellipsis()], int),
        f("setvbuf", vec![__todo], __todo),
        f("snprintf", vec![voidp, size, charp, &ellipsis()], __todo),
        f("sprintf", vec![charp, charp, &ellipsis()], __todo),
        f("sscanf", vec![voidp, charp, &ellipsis()], int),
        f("tmpfile", vec![], filep),
        f("ungetc", vec![int, filep], int),
        f("vfprintf", vec![filep, charp, __todo], int),
        f("vprintf", vec![charp, __todo], int),
        f("vsnprintf", vec![voidp, size, charp, __todo], int),
        f("vsprintf", vec![voidp, charp, __todo], int),
        t("FILE"),
        // ctype
        f("isalpha", vec![int], bool),
        f("isdigit", vec![int], bool),
        f("isgraph", vec![int], bool),
        f("islower", vec![int], bool),
        f("isprint", vec![int], bool),
        f("isspace", vec![int], bool),
        f("tolower", vec![int], int),
        f("toupper", vec![int], int),
        // stdlib
        f("abort", vec![__todo], void), // depr
        f("atof", vec![__todo], __todo),
        f("atoi", vec![__todo], __todo),
        f("atol_l", vec![__todo], __todo),
        f("atol", vec![__todo], __todo),
        f("atoll_l", vec![__todo], __todo),
        f("atoll", vec![__todo], __todo),
        f("calloc", vec![size, size], voidp),
        f("exit", vec![int], void),
        f("free", vec![voidp], void),
        f("qsort", vec![voidp, size, size, __todo], void),
        f("realloc", vec![voidp, size], voidp),
        f("strtod", vec![__todo, __todo], __todo),
        f("strtof", vec![__todo], __todo),
        f("strtold", vec![__todo], __todo),
        f("strtoull", vec![__todo, __todo, __todo], __todo),
        // math
        c("M_PI", dbl),
        f("acos", vec![dbl], dbl),
        f("ceil", vec![dbl], dbl),
        f("ceilf", vec![flt], flt),
        f("cos", vec![dbl], dbl),
        f("cosf", vec![flt], flt),
        f("exp", vec![dbl], dbl),
        f("expm1", vec![dbl], dbl),
        f("fabs", vec![dbl], dbl),
        f("floor", vec![dbl], dbl),
        f("floorf", vec![flt], flt),
        f("fmod", vec![dbl, dbl], dbl),
        f("lgamma", vec![dbl], dbl),
        f("log", vec![dbl], dbl),
        f("log10", vec![dbl], dbl),
        f("log1p", vec![dbl], dbl),
        f("pow", vec![dbl, dbl], dbl),
        f("round", vec![dbl], dbl),
        f("roundf", vec![flt], flt),
        f("sin", vec![dbl], dbl),
        f("sinf", vec![flt], flt),
        f("sqrt", vec![dbl], dbl),
        f("sqrtf", vec![flt], flt),
        // string
        f("memcmp", vec![voidp, voidp, size], int),
        f("memcpy", vec![voidp, voidp, size], void),
        f("memmove", vec![voidp, voidp, size], void),
        f("memset", vec![voidp, int, size], void),
        f("strcat", vec![charp, charp], charp),
        f("strchr", vec![charp, int], charp),
        f("strcmp", vec![charp, charp], int),
        f("strcpy", vec![charp, charp], charp),
        f("strerror", vec![int], charp),
        f("strlen", vec![charp], size),
        f("strncat", vec![charp, charp, size], chr),
        f("strncmp", vec![charp, charp, size], int),
        // f("strncpy", vec![charp, charp, size], chr), // depr, weird
        f("strrchr", vec![charp, int], charp),
        f("strstr", vec![charp, charp], charp),
        // time
        t("time_t"),
        f("strftime", vec![__todo, __todo, __todo, __todo], __todo), // depr
        f("localtime", vec![__todo], __todo),                        // struct tm, depr
        f("time", vec![__todo], &just("time_t")),                    // depr
        // setjmp
        t("jmp_buf"),
        f("longjmp", vec![__todo, __todo], __todo),
        f("setjmp", vec![__todo], __todo),
        // stdarg
        t("va_list"),
        f("va_end", vec![__todo], void),
        f("va_start", vec![__todo, __todo], void),
        // signal
        c("SIGABRT", int),
        c("SIGFPE", int),
        c("SIGILL", int),
        c("SIGINT", int),
        c("SIGSEGV", int),
        c("SIGTERM", int),
        c("SIG_DFL", int),
        c("SIG_IGN", int),
        f("signal", vec![__todo, __todo], int), // depr
        // assert
        f("assert", vec![__todo], void), // depr
        // errno
        c("errno", int),
        //
        // extensions
        //
        f("nelem", vec![__todo], size),
        f("calloc_or_panic", vec![size, size], voidp),
    ];
    let mut m = HashMap::new();
    for x in list {
        m.insert(x.name, x);
    }
    m
}
