use crate::types::{Bytes, Signedness, Type};

pub struct LibEntry<'a> {
    pub t: Type,
    pub header: &'a str,
}

fn le(header: &str, t: Type) -> Option<LibEntry> {
    Some(LibEntry { t, header })
}
fn by(sign: Signedness, size: usize) -> Type {
    Type::Bytes(Bytes { sign, size })
}
fn obj() -> Type {
    Type::Struct {
        opaque: true,
        fields: Vec::new(),
    }
}

pub fn get_type(name: &String) -> Option<LibEntry> {
    match name.as_str() {
        // plain C
        "float" => le("", Type::Float),
        "double" => le("", Type::Double),
        "void" => le("", Type::Void),
        "char" => le("", by(Signedness::Unknonwn, 8)),
        "int" => le("", by(Signedness::Signed, 0)),
        "long" => le("", by(Signedness::Signed, 0)),
        "short" => le("", by(Signedness::Signed, 0)),
        "unsigned" => le("", by(Signedness::Unsigned, 0)),
        // stdint.h
        "int8_t" => le("stdint.h", by(Signedness::Signed, 8)),
        "int16_t" => le("stdint.h", by(Signedness::Signed, 16)),
        "int32_t" => le("stdint.h", by(Signedness::Signed, 32)),
        "int64_t" => le("stdint.h", by(Signedness::Signed, 64)),
        "uint8_t" => le("stdint.h", by(Signedness::Unsigned, 8)),
        "uint16_t" => le("stdint.h", by(Signedness::Unsigned, 16)),
        "uint32_t" => le("stdint.h", by(Signedness::Unsigned, 32)),
        "uint64_t" => le("stdint.h", by(Signedness::Unsigned, 64)),
        // stdbool.h
        "bool" => le("stdbool.h", Type::Bool),
        // stdio.h
        "FILE" => le("stdio.h", obj()),
        // ...
        "clock_t" => le("?", by(Signedness::Unsigned, 0)),
        "ptrdiff_t" => le("?", by(Signedness::Signed, 0)),
        "size_t" => le("?", by(Signedness::Unsigned, 0)),
        "time_t" => le("?", by(Signedness::Unknonwn, 0)),
        "jmp_buf" => le("?", obj()),
        "va_list" => le("?", obj()),
        "wchar_t" => le("?", obj()),
        _ => None,
    }
}

pub const CLIBS: &[&str] = &[
    "assert", "ctype", "errno", "limits", "math", "stdarg", "stdbool", "stddef", "stdint", "stdio",
    "stdlib", "string", "time", "setjmp", "signal",
];
pub const CTYPES: &[&str] = &[
    "bool",
    "char",
    "clock_t",
    "double",
    "FILE",
    "float",
    "int",
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "jmp_buf",
    "long",
    "ptrdiff_t",
    "short",
    "size_t",
    "time_t",
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "unsigned",
    "va_list",
    "void",
    "wchar_t",
];
pub const CCONST: &[&str] = &[
    "RAND_MAX", "BUFSIZ", "_IOLBF", "EOF", "SEEK_SET", "SEEK_END", "SEEK_CUR", "errno",
    // limits.h
    "INT_MAX", "LONG_MAX", "SIZE_MAX", // signal.h
    "SIGABRT", "SIGFPE", "SIGILL", "SIGINT", "SIGSEGV", "SIGTERM", "SIG_DFL", "SIG_IGN",
    // math.h
    "M_PI",
];

pub const CFUNCS: &[&str] = &[
    // ctype
    "isgraph",
    "islower",
    "isspace",
    "isalpha",
    "tolower",
    "toupper",
    "isprint",
    // stdlib
    "atoi",
    "atol",
    "atoll",
    "atol_l",
    "atoll_l",
    "strtod",
    "strtof",
    "strtold",
    // string.h
    "memcmp",
    // stdio.h
    "printf",
    "puts",
    // signal.h
    "signal",
    // ...
    "strncat",
    "acos",
    "floor",
    "ceil",
    "round",
    "strrchr",
    "strncmp",
    "setjmp",
    "longjmp",
    "strcat",
    "floorf",
    "sqrtf",
    "ceilf",
    "sinf",
    "cosf",
    "fabs",
    "fmod",
    "strtoull",
    "strncpy",
    "rewind",
    "qsort",
    "strchr",
    "fgets",
    "log",
    "sqrt",
    "assert",
    "vasprintf",
    "fputs",
    "rename",
    "setvbuf",
    "strstr",
    "remove",
    "calloc",
    "cos",
    "exit",
    "exp",
    "fclose",
    "feof",
    "fgetc",
    "fopen",
    "fprintf",
    "fputc",
    "fseek",
    "ftell",
    "fread",
    "free",
    "fwrite",
    "localtime",
    // "malloc",
    "memcpy",
    "memset",
    "realloc",
    "roundf",
    "ungetc",
    "putchar",
    "scanf",
    "abort",
    "memmove",
    "srand",
    "rand",
    "getc",
    "getchar",
    "putc",
    "fflush",
    "ferror",
    "sin",
    "snprintf",
    "strcmp",
    "strcpy",
    "strlen",
    "stdin",
    "stdout",
    "stderr",
    "time",
    "tmpfile",
    "va_end",
    "va_start",
    "vfprintf",
    "vprintf",
    "vsnprintf",
    "vsprintf",
    "sprintf",
    "strftime",
    "strerror",
    "sscanf",
    "isdigit",
];
