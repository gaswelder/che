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

// A wrapper to declare a built-in constant.
fn c(name: &'static str, t: types::Type) -> CSymbol {
    CSymbol {
        kind: Kind::SYM,
        name,
        t,
    }
}

// A wrapper to declare a built-in function.
fn f(name: &'static str, t: types::Type) -> CSymbol {
    CSymbol {
        kind: Kind::SYM,
        name,
        t,
    }
}

fn ret(args: Vec<types::Type>, rettypename: &str) -> types::Type {
    types::Type {
        base: nodes::NsName {
            ns: "".to_string(),
            name: rettypename.to_string(),
            pos: Pos { line: 0, col: 0 },
        },
        ops: vec![types::TypeOp::Call(args)],
    }
}

fn retp(args: Vec<types::Type>, rettypename: &str) -> types::Type {
    types::Type {
        base: nodes::NsName {
            ns: "".to_string(),
            name: rettypename.to_string(),
            pos: Pos { line: 0, col: 0 },
        },
        ops: vec![types::TypeOp::Call(args), types::TypeOp::Deref],
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
        f("fclose", ret(vec![types::justp("FILE")], "int")),
        f("feof", ret(vec![types::justp("FILE")], "bool")),
        f("ferror", ret(vec![types::justp("FILE")], "bool")),
        f("fflush", ret(vec![types::justp("FILE")], "int")),
        f("fgetc", ret(vec![types::justp("FILE")], "int")),
        f("fgets", retp(vec![types::justp("FILE")], "char")),
        f(
            "fopen",
            retp(vec![types::justp("char"), types::justp("char")], "FILE"),
        ),
        f("fprintf", ret(vec![types::unk()], "void")),
        f("fputc", ret(vec![types::unk()], "int")),
        f("fputs", ret(vec![types::unk()], "int")),
        f("fread", ret(vec![types::unk()], "size_t")),
        f("fseek", ret(vec![types::unk()], "int")),
        f("ftell", ret(vec![types::unk()], "int32_t")),
        f("fwrite", ret(vec![types::unk()], "size_t")),
        f("getc", ret(vec![types::unk()], "int")),
        f("getchar", ret(vec![types::unk()], "int")),
        f("printf", ret(vec![types::unk()], "int")),
        f("putc", types::unk()),
        f("putchar", types::unk()),
        f("puts", ret(vec![types::unk()], "int")),
        f("remove", types::unk()),                   // depr
        f("rename", ret(vec![types::unk()], "int")), // depr
        f("rewind", types::unk()),
        f("scanf", types::unk()),
        f("setvbuf", types::unk()),
        f("snprintf", types::unk()),
        f("sprintf", types::unk()),
        f("sscanf", types::unk()),
        f("tmpfile", retp(vec![types::unk()], "FILE")),
        f("ungetc", ret(vec![types::unk()], "int")),
        f("vfprintf", ret(vec![types::unk()], "int")),
        f("vprintf", ret(vec![types::unk()], "int")),
        f("vsnprintf", ret(vec![types::unk()], "int")),
        f("vsprintf", ret(vec![types::unk()], "int")),
        t("FILE"),
        // ctype
        f("isalpha", ret(vec![types::just("int")], "bool")),
        f("isdigit", ret(vec![types::just("int")], "bool")),
        f("isgraph", ret(vec![types::just("int")], "bool")),
        f("islower", ret(vec![types::just("int")], "bool")),
        f("isprint", ret(vec![types::just("int")], "bool")),
        f("isspace", ret(vec![types::just("int")], "bool")),
        f("tolower", ret(vec![types::just("int")], "int")),
        f("toupper", ret(vec![types::just("int")], "int")),
        // stdlib
        c("RAND_MAX", types::just("int")),           // depr
        f("abort", ret(vec![types::unk()], "void")), // depr
        f("atof", types::unk()),
        f("atoi", types::unk()),
        f("atol_l", types::unk()),
        f("atol", types::unk()),
        f("atoll_l", types::unk()),
        f("atoll", types::unk()),
        f("calloc", retp(vec![types::unk()], "void")),
        f("exit", ret(vec![types::unk()], "void")),
        f("free", ret(vec![types::unk()], "void")),
        f("qsort", types::unk()),
        f("rand", ret(vec![], "int")), // depr
        f("realloc", types::unk()),
        f("srand", types::unk()), // depr
        f("strtod", types::unk()),
        f("strtof", types::unk()),
        f("strtold", types::unk()),
        f("strtoull", types::unk()),
        // math
        c("M_PI", types::unk()),
        f("acos", ret(vec![types::just("double")], "double")),
        f("ceil", ret(vec![types::just("double")], "double")),
        f("ceilf", ret(vec![types::just("float")], "float")),
        f("cos", ret(vec![types::just("double")], "double")),
        f("cosf", ret(vec![types::just("float")], "float")),
        f("exp", ret(vec![types::just("double")], "double")),
        f("expm1", ret(vec![types::just("double")], "double")),
        f("fabs", ret(vec![types::just("double")], "double")),
        f("floor", ret(vec![types::just("double")], "double")),
        f("floorf", ret(vec![types::just("float")], "float")),
        f("fmod", ret(vec![types::just("double")], "double")),
        f("lgamma", ret(vec![types::just("double")], "double")),
        f("log", ret(vec![types::just("double")], "double")),
        f("log1p", ret(vec![types::just("double")], "double")),
        f("round", types::unk()),
        f("roundf", types::unk()),
        f("sin", types::unk()),
        f("sinf", types::unk()),
        f("sqrt", ret(vec![types::just("double")], "double")),
        f("sqrtf", ret(vec![types::just("float")], "float")),
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
        f("strlen", ret(vec![types::justp("char")], "size_t")),
        f("strncat", retp(vec![types::unk()], "char")),
        f("strncmp", types::unk()),
        f("strncpy", retp(vec![types::unk()], "char")),
        f("strrchr", types::unk()),
        f("strstr", types::unk()),
        // time
        t("time_t"),
        f("strftime", types::unk()),
        f("localtime", types::unk()),                 // struct tm, depr
        f("time", ret(vec![types::unk()], "time_t")), // depr
        // setjmp
        t("jmp_buf"),
        f("longjmp", types::unk()),
        f("setjmp", types::unk()),
        // stdarg
        t("va_list"),
        f("va_end", ret(vec![types::unk()], "void")),
        f("va_start", ret(vec![types::unk()], "void")),
        // signal
        c("SIGABRT", types::just("int")),
        c("SIGFPE", types::just("int")),
        c("SIGILL", types::just("int")),
        c("SIGINT", types::just("int")),
        c("SIGSEGV", types::just("int")),
        c("SIGTERM", types::just("int")),
        c("SIG_DFL", types::just("int")),
        c("SIG_IGN", types::just("int")),
        f("signal", ret(vec![types::unk()], "int")), // depr
        // assert
        f("assert", ret(vec![types::unk()], "void")), // depr
        // errno
        c("errno", types::just("int")),
        //
        // extensions
        //
        f("nelem", ret(vec![types::unk()], "size_t")),
    ];
    let mut m = HashMap::new();
    for x in list {
        m.insert(x.name, x);
    }
    m
}
