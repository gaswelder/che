use std::{collections::HashMap, sync::OnceLock};

use crate::{
    buf::Pos,
    nodes,
    types::{just, justp, todo, Type, TypeOp},
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
fn c(name: &'static str, t: Type) -> CSymbol {
    CSymbol {
        kind: Kind::SYM,
        name,
        t,
    }
}

// A wrapper to declare a built-in function.
fn f(name: &'static str, args: Vec<Type>, mut rettype: Type) -> CSymbol {
    rettype.ops.insert(0, TypeOp::Call(args));
    CSymbol {
        kind: Kind::SYM,
        name,
        t: rettype,
    }
}

fn buildmap() -> HashMap<&'static str, CSymbol> {
    let list = vec![
        // native
        t("char"),
        t("double"),
        t("float"),
        t("int"),
        t("void"),
        // stddef
        c("NULL", justp("void")),
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
        c("false", just("bool")),
        c("true", just("bool")),
        // limits
        c("INT_MAX", just("int")),
        c("SIZE_MAX", just("size_t")),
        // stdio
        c("BUFSIZ", just("size_t")),
        c("SEEK_END", just("int")),
        c("SEEK_SET", just("int")),
        c("SEEK_CUR", just("int")),
        c("EOF", just("int")),
        c("stderr", todo()),
        c("stdin", todo()),
        c("stdout", todo()),
        f("fclose", vec![justp("FILE")], just("int")),
        f("feof", vec![justp("FILE")], just("bool")),
        f("ferror", vec![justp("FILE")], just("bool")),
        f("fflush", vec![justp("FILE")], just("int")),
        f("fgetc", vec![justp("FILE")], just("int")),
        f("fgets", vec![justp("FILE")], justp("char")),
        f("fopen", vec![justp("char"), justp("char")], just("FILE")),
        f("fprintf", vec![todo()], just("void")),
        f("fputc", vec![todo()], just("int")),
        f("fputs", vec![todo()], just("int")),
        f("fread", vec![todo()], just("size_t")),
        f("fseek", vec![todo()], just("int")),
        f("ftell", vec![todo()], just("int32_t")),
        f("fwrite", vec![todo()], just("size_t")),
        f("getc", vec![todo()], just("int")),
        f("getchar", vec![todo()], just("int")),
        f("printf", vec![todo()], just("int")),
        f("putc", vec![todo()], todo()),
        f("putchar", vec![todo()], todo()),
        f("puts", vec![todo()], just("int")),
        f("remove", vec![todo()], todo()),      // depr
        f("rename", vec![todo()], just("int")), // depr
        f("rewind", vec![todo()], todo()),
        f("scanf", vec![todo()], todo()),
        f("setvbuf", vec![todo()], todo()),
        f("snprintf", vec![todo()], todo()),
        f("sprintf", vec![todo()], todo()),
        f("sscanf", vec![todo()], todo()),
        f("tmpfile", vec![todo()], justp("FILE")),
        f("ungetc", vec![todo()], just("int")),
        f("vfprintf", vec![todo()], just("int")),
        f("vprintf", vec![todo()], just("int")),
        f("vsnprintf", vec![todo()], just("int")),
        f("vsprintf", vec![todo()], just("int")),
        t("FILE"),
        // ctype
        f("isalpha", vec![just("int")], just("bool")),
        f("isdigit", vec![just("int")], just("bool")),
        f("isgraph", vec![just("int")], just("bool")),
        f("islower", vec![just("int")], just("bool")),
        f("isprint", vec![just("int")], just("bool")),
        f("isspace", vec![just("int")], just("bool")),
        f("tolower", vec![just("int")], just("int")),
        f("toupper", vec![just("int")], just("int")),
        // stdlib
        f("abort", vec![todo()], just("void")), // depr
        f("atof", vec![todo()], todo()),
        f("atoi", vec![todo()], todo()),
        f("atol_l", vec![todo()], todo()),
        f("atol", vec![todo()], todo()),
        f("atoll_l", vec![todo()], todo()),
        f("atoll", vec![todo()], todo()),
        f("calloc", vec![todo()], justp("void")),
        f("exit", vec![todo()], just("void")),
        f("free", vec![todo()], just("void")),
        f("qsort", vec![todo()], todo()),
        f("realloc", vec![todo()], todo()),
        f("strtod", vec![todo()], todo()),
        f("strtof", vec![todo()], todo()),
        f("strtold", vec![todo()], todo()),
        f("strtoull", vec![todo()], todo()),
        // math
        c("M_PI", todo()),
        f("acos", vec![just("double")], just("double")),
        f("ceil", vec![just("double")], just("double")),
        f("ceilf", vec![just("float")], just("float")),
        f("cos", vec![just("double")], just("double")),
        f("cosf", vec![just("float")], just("float")),
        f("exp", vec![just("double")], just("double")),
        f("expm1", vec![just("double")], just("double")),
        f("fabs", vec![just("double")], just("double")),
        f("floor", vec![just("double")], just("double")),
        f("floorf", vec![just("float")], just("float")),
        f("fmod", vec![just("double")], just("double")),
        f("lgamma", vec![just("double")], just("double")),
        f("log", vec![just("double")], just("double")),
        f("log1p", vec![just("double")], just("double")),
        f("round", vec![todo()], todo()),
        f("roundf", vec![todo()], todo()),
        f("sin", vec![todo()], todo()),
        f("sinf", vec![todo()], todo()),
        f("sqrt", vec![just("double")], just("double")),
        f("sqrtf", vec![just("float")], just("float")),
        // string
        f("memcmp", vec![todo()], todo()),
        f("memcpy", vec![todo()], todo()),
        f("memmove", vec![todo()], todo()),
        f("memset", vec![todo()], todo()),
        f("strcat", vec![todo()], todo()),
        f("strchr", vec![todo()], todo()),
        f("strcmp", vec![todo()], todo()),
        f("strcpy", vec![todo()], todo()),
        f("strerror", vec![todo()], todo()),
        f("strlen", vec![justp("char")], just("size_t")),
        f("strncat", vec![todo()], justp("char")),
        f("strncmp", vec![todo()], todo()),
        f("strncpy", vec![todo()], justp("char")),
        f("strrchr", vec![todo()], todo()),
        f("strstr", vec![todo()], todo()),
        // time
        t("time_t"),
        f("strftime", vec![todo()], todo()),
        f("localtime", vec![todo()], todo()), // struct tm, depr
        f("time", vec![todo()], just("time_t")), // depr
        // setjmp
        t("jmp_buf"),
        f("longjmp", vec![todo()], todo()),
        f("setjmp", vec![todo()], todo()),
        // stdarg
        t("va_list"),
        f("va_end", vec![todo()], just("void")),
        f("va_start", vec![todo()], just("void")),
        // signal
        c("SIGABRT", just("int")),
        c("SIGFPE", just("int")),
        c("SIGILL", just("int")),
        c("SIGINT", just("int")),
        c("SIGSEGV", just("int")),
        c("SIGTERM", just("int")),
        c("SIG_DFL", just("int")),
        c("SIG_IGN", just("int")),
        f("signal", vec![todo()], just("int")), // depr
        // assert
        f("assert", vec![todo()], just("void")), // depr
        // errno
        c("errno", just("int")),
        //
        // extensions
        //
        f("nelem", vec![todo()], just("size_t")),
    ];
    let mut m = HashMap::new();
    for x in list {
        m.insert(x.name, x);
    }
    m
}
