use std::{collections::HashMap, sync::OnceLock};

use crate::{buf::Pos, nodes, types};

#[derive(Debug)]
pub enum Kind {
    TYPE,
    SYM,
}

#[derive(Debug)]
pub struct CSymbol {
    pub kind: Kind,
    pub name: &'static str,
    pub t: types::Type,
}

static SYMS: OnceLock<HashMap<&str, CSymbol>> = OnceLock::new();

pub fn has_type(name: &str) -> bool {
    find_sym(name).is_some_and(|x| matches!(x.kind, Kind::TYPE))
}

pub fn find_sym(s: &str) -> Option<&CSymbol> {
    getsyms().get(s)
}

fn getsyms() -> &'static HashMap<&'static str, CSymbol> {
    SYMS.get_or_init(buildmap)
}

fn t(name: &'static str) -> CSymbol {
    CSymbol {
        kind: Kind::TYPE,
        name,
        t: types::Type {
            base: nodes::NsName {
                ns: "".to_string(),
                name: name.to_string(),
                pos: Pos { col: 0, line: 0 },
            },
            ops: Vec::new(),
        },
    }
}

fn ret(name: &str) -> types::Type {
    types::Type {
        base: nodes::NsName {
            ns: "".to_string(),
            name: name.to_string(),
            pos: Pos { line: 0, col: 0 },
        },
        ops: vec![types::TypeOp::Call],
    }
}

fn retp(name: &str) -> types::Type {
    types::Type {
        base: nodes::NsName {
            ns: "".to_string(),
            name: name.to_string(),
            pos: Pos { line: 0, col: 0 },
        },
        ops: vec![types::TypeOp::Call, types::TypeOp::Deref],
    }
}

// Constructs a constant definition.
fn c(name: &'static str, t: types::Type) -> CSymbol {
    CSymbol {
        kind: Kind::SYM,
        name,
        t,
    }
}

// Constructs a function definition.
fn f(name: &'static str, t: types::Type) -> CSymbol {
    CSymbol {
        kind: Kind::SYM,
        name,
        t,
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
        c("NULL", types::justp("void")),
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
        c("false", types::just("bool")),
        c("true", types::just("bool")),
        // limits
        c("INT_MAX", types::just("int")),
        c("SIZE_MAX", types::just("size_t")),
        // stdio
        c("BUFSIZ", types::just("size_t")),
        c("SEEK_END", types::just("int")),
        c("SEEK_SET", types::just("int")),
        c("SEEK_CUR", types::just("int")),
        c("EOF", types::just("int")),
        c("stderr", types::unk()),
        c("stdin", types::unk()),
        c("stdout", types::unk()),
        f("fclose", ret("int")),
        f("feof", ret("bool")),
        f("ferror", ret("bool")),
        f("fflush", ret("int")),
        f("fgetc", ret("int")),
        f("fgets", retp("char")),
        f("fopen", retp("FILE")),
        f("fprintf", ret("void")),
        f("fputc", ret("int")),
        f("fputs", ret("int")),
        f("fread", ret("size_t")),
        f("fseek", ret("int")),
        f("ftell", ret("int32_t")),
        f("fwrite", ret("size_t")),
        f("getc", ret("int")),
        f("getchar", ret("int")),
        f("printf", ret("int")),
        f("putc", types::unk()),
        f("putchar", types::unk()),
        f("puts", ret("int")),
        f("remove", types::unk()),
        f("rename", ret("int")),
        f("rewind", types::unk()),
        f("scanf", types::unk()),
        f("setvbuf", types::unk()),
        f("snprintf", types::unk()),
        f("sprintf", types::unk()),
        f("sscanf", types::unk()),
        f("tmpfile", retp("FILE")),
        f("ungetc", ret("int")),
        f("vfprintf", ret("int")),
        f("vprintf", ret("int")),
        f("vsnprintf", ret("int")),
        f("vsprintf", ret("int")),
        t("FILE"),
        // ctype
        f("isalpha", ret("bool")),
        f("isdigit", ret("bool")),
        f("isgraph", ret("bool")),
        f("islower", ret("bool")),
        f("isprint", ret("bool")),
        f("isspace", ret("bool")),
        f("tolower", ret("int")),
        f("toupper", ret("int")),
        // stdlib
        c("RAND_MAX", types::just("int")), // depr
        f("abort", ret("void")),
        f("atof", types::unk()),
        f("atoi", types::unk()),
        f("atol_l", types::unk()),
        f("atol", types::unk()),
        f("atoll_l", types::unk()),
        f("atoll", types::unk()),
        f("calloc", retp("void")),
        f("exit", ret("void")),
        f("free", ret("void")),
        f("qsort", types::unk()),
        f("rand", ret("int")), // depr
        f("realloc", types::unk()),
        f("srand", types::unk()), // depr
        f("strtod", types::unk()),
        f("strtof", types::unk()),
        f("strtold", types::unk()),
        f("strtoull", types::unk()),
        // math
        c("M_PI", types::unk()),
        f("acos", ret("double")),
        f("ceil", ret("double")),
        f("ceilf", ret("float")),
        f("cos", ret("double")),
        f("cosf", ret("float")),
        f("exp", ret("double")),
        f("expm1", ret("double")),
        f("fabs", ret("double")),
        f("floor", ret("double")),
        f("floorf", ret("float")),
        f("fmod", ret("double")),
        f("lgamma", ret("double")),
        f("log", ret("double")),
        f("log1p", ret("double")),
        f("round", types::unk()),
        f("roundf", types::unk()),
        f("sin", types::unk()),
        f("sinf", types::unk()),
        f("sqrt", ret("double")),
        f("sqrtf", ret("float")),
        // string
        f("memcmp", types::unk()),
        f("memcpy", types::unk()),
        f("memmove", types::unk()),
        f("memset", types::unk()),
        f("strcat", types::unk()),
        f("strchr", types::unk()),
        f("strcmp", types::unk()),
        f("strcpy", types::unk()),
        f("strerror", types::unk()),
        f("strlen", ret("size_t")),
        f("strncat", retp("char")),
        f("strncmp", types::unk()),
        f("strncpy", retp("char")),
        f("strrchr", types::unk()),
        f("strstr", types::unk()),
        // time
        t("time_t"),
        f("strftime", types::unk()),
        f("localtime", types::unk()), // struct tm, depr
        f("time", ret("time_t")),
        // setjmp
        t("jmp_buf"),
        f("longjmp", types::unk()),
        f("setjmp", types::unk()),
        // stdarg
        t("va_list"),
        f("va_end", ret("void")),
        f("va_start", ret("void")),
        // signal
        c("SIGABRT", types::just("int")),
        c("SIGFPE", types::just("int")),
        c("SIGILL", types::just("int")),
        c("SIGINT", types::just("int")),
        c("SIGSEGV", types::just("int")),
        c("SIGTERM", types::just("int")),
        c("SIG_DFL", types::just("int")),
        c("SIG_IGN", types::just("int")),
        f("signal", ret("int")), // depr
        // assert
        f("assert", ret("void")), // depr
        // errno
        c("errno", types::just("int")),
        //
        // extensions
        //
        f("nelem", ret("size_t")),
    ];

    // , , "_IOLBF", , , ,

    let mut m = HashMap::new();
    for x in list {
        m.insert(x.name, x);
    }
    m
}
