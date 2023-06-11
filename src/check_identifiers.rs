use crate::{c, exports::Exports, nodes::*, parser::Error};
use std::collections::HashMap;

pub fn run(m: &Module, exports: &HashMap<String, &Exports>) -> Vec<Error> {
    let mut errors: Vec<Error> = Vec::new();
    let mut scope = vec![
        // should be fixed
        "assert",
        "break",
        "continue",
        // stdlib
        "acos",
        "round",
        "islower",
        "isspace",
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
        "strtoull",
        "strncpy",
        "rewind",
        "qsort",
        "strchr",
        "fgets",
        "log",
        "sqrt",
        "RAND_MAX",
        "vasprintf",
        "fputs",
        "rename",
        "setvbuf",
        "BUFSIZ",
        "_IOLBF",
        "isalpha",
        "strstr",
        "remove",
        "calloc",
        "cos",
        "EOF",
        "exit",
        "exp",
        "false",
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
        "malloc",
        "memcpy",
        "memset",
        "NULL",
        "printf",
        "puts",
        "realloc",
        "roundf",
        "SEEK_SET",
        "SEEK_END",
        "SEEK_CUR",
        "ungetc",
        "putchar",
        "scanf",
        "abort",
        "memmove",
        "srand",
        "rand",
        "getc",
        "signal",
        "SIGINT",
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
        "true",
        "va_end",
        "va_start",
        "vfprintf",
        "vprintf",
        "vsnprintf",
        "sprintf",
        "strftime",
        "errno",
        "strerror",
        "SIZE_MAX",
        "sscanf",
        "isdigit",
        // custom
        "nelem",
    ];
    for s in c::CTYPES {
        scope.push(s);
    }
    for e in &m.elements {
        match e {
            ModuleObject::FunctionDeclaration(f) => {
                scope.push(f.form.name.as_str());
            }
            ModuleObject::Enum { is_pub: _, members } => {
                for m in members {
                    scope.push(m.id.name.as_str());
                }
            }
            ModuleObject::Macro { name, value } => {
                if name == "define" {
                    let mut parts = value.trim().split(" ");
                    scope.push(parts.next().unwrap());
                }
                if name == "type" {
                    scope.push(value.trim());
                }
            }
            ModuleObject::ModuleVariable {
                type_name: _,
                form,
                value: _,
            } => {
                scope.push(form.name.as_str());
            }
            ModuleObject::StructTypedef(x) => {
                scope.push(x.name.name.as_str());
            }
            ModuleObject::Typedef(x) => {
                scope.push(x.alias.name.as_str());
            }
            ModuleObject::StructAliasTypedef {
                is_pub: _,
                struct_name: _,
                type_alias,
            } => {
                scope.push(type_alias.as_str());
            }
        }
    }

    for e in &m.elements {
        match e {
            ModuleObject::Typedef(x) => {
                check_ns_id(&x.type_name.name, &mut errors, &scope, exports);
            }
            ModuleObject::Enum { is_pub: _, members } => {
                for m in members {
                    if m.value.is_some() {
                        check_expr(m.value.as_ref().unwrap(), &mut errors, &scope, exports);
                    }
                }
            }
            ModuleObject::FunctionDeclaration(f) => {
                check_ns_id(&f.type_name.name, &mut errors, &scope, exports);
                let mut function_scope = scope.clone();
                for pl in &f.parameters.list {
                    check_ns_id(&pl.type_name.name, &mut errors, &scope, exports);
                    for f in &pl.forms {
                        function_scope.push(&f.name);
                    }
                }
                check_body(&f.body, &mut errors, &function_scope, exports);
            }
            ModuleObject::Macro { name, value } => {
                if name == "known" {
                    scope.push(value.trim());
                }
            }
            ModuleObject::ModuleVariable {
                type_name,
                form: _,
                value,
            } => {
                check_ns_id(&type_name.name, &mut errors, &scope, exports);
                check_expr(value, &mut errors, &scope, exports);
            }
            ModuleObject::StructAliasTypedef { .. } => {}
            ModuleObject::StructTypedef(x) => {
                for f in &x.fields {
                    match f {
                        StructEntry::Plain(x) => {
                            check_ns_id(&x.type_name.name, &mut errors, &scope, exports);
                        }
                        StructEntry::Union(_) => {}
                    }
                }
            }
        }
    }
    return errors;
}

fn check_body(
    body: &Body,
    errors: &mut Vec<Error>,
    parent_scope: &Vec<&str>,
    exports: &HashMap<String, &Exports>,
) {
    let mut scope: Vec<&str> = parent_scope.clone();
    for s in &body.statements {
        match s {
            Statement::Expression(e) => {
                check_expr(e, errors, &scope, exports);
            }
            Statement::For {
                init,
                condition,
                action,
                body,
            } => {
                let loop_scope = &mut scope.clone();
                match init {
                    ForInit::LoopCounterDeclaration {
                        type_name,
                        form,
                        value,
                    } => {
                        check_ns_id(&type_name.name, errors, loop_scope, exports);
                        check_expr(value, errors, loop_scope, exports);
                        loop_scope.push(form.name.as_str());
                    }
                    ForInit::Expression(x) => {
                        check_expr(x, errors, loop_scope, exports);
                    }
                }
                check_expr(condition, errors, loop_scope, exports);
                check_expr(action, errors, loop_scope, exports);
                check_body(body, errors, loop_scope, exports);
            }
            Statement::If {
                condition,
                body,
                else_body,
            } => {
                check_expr(condition, errors, &scope, exports);
                check_body(body, errors, &scope, exports);
                if else_body.is_some() {
                    check_body(else_body.as_ref().unwrap(), errors, &scope, exports);
                }
            }
            Statement::Return { expression } => match expression {
                Some(x) => {
                    check_expr(x, errors, &scope, exports);
                }
                None => {}
            },
            Statement::Switch {
                value,
                cases,
                default,
            } => {
                check_expr(value, errors, &scope, exports);
                for c in cases {
                    match &c.value {
                        SwitchCaseValue::Identifier(x) => {
                            check_ns_id(x, errors, &scope, exports);
                        }
                        SwitchCaseValue::Literal(_) => {}
                    }
                    check_body(&c.body, errors, &scope, exports);
                }
                if default.is_some() {
                    check_body(default.as_ref().unwrap(), errors, &scope, exports);
                }
            }
            Statement::VariableDeclaration {
                type_name,
                forms,
                values,
            } => {
                check_ns_id(&type_name.name, errors, &scope, exports);
                for v in values {
                    if v.is_some() {
                        check_expr(v.as_ref().unwrap(), errors, &scope, exports);
                    }
                }
                for f in forms {
                    scope.push(f.name.as_str());
                }
            }
            Statement::While { condition, body } => {
                check_expr(condition, errors, &scope, exports);
                check_body(body, errors, &scope, exports);
            }
        }
    }
}

fn check_expr(
    e: &Expression,
    errors: &mut Vec<Error>,
    scope: &Vec<&str>,
    exports: &HashMap<String, &Exports>,
) {
    match e {
        Expression::ArrayIndex { array, index } => {
            check_expr(array, errors, scope, exports);
            check_expr(index, errors, scope, exports);
        }
        Expression::BinaryOp { op, a, b } => {
            if op != "->" && op != "." {
                check_expr(a, errors, scope, exports);
                check_expr(b, errors, scope, exports);
            }
        }
        Expression::Cast { type_name, operand } => {
            check_ns_id(&type_name.type_name.name, errors, scope, exports);
            check_expr(operand, errors, scope, exports);
        }
        Expression::CompositeLiteral(x) => {
            for e in &x.entries {
                check_expr(&e.value, errors, scope, exports);
            }
        }
        Expression::FunctionCall {
            function,
            arguments,
        } => {
            check_expr(function, errors, scope, exports);
            for x in arguments {
                check_expr(x, errors, scope, exports);
            }
        }
        Expression::Identifier(x) => {
            check_id(x, errors, scope);
        }
        Expression::Literal(_) => {}
        Expression::NsName(x) => {
            check_ns_id(x, errors, scope, exports);
        }
        Expression::PostfixOperator {
            operator: _,
            operand,
        } => {
            check_expr(operand, errors, scope, exports);
        }
        Expression::PrefixOperator {
            operator: _,
            operand,
        } => {
            check_expr(operand, errors, scope, exports);
        }
        Expression::Sizeof { argument } => {
            match argument.as_ref() {
                SizeofArgument::Typename(_) => {
                    //
                }
                SizeofArgument::Expression(x) => {
                    check_expr(&x, errors, scope, exports);
                }
            }
        }
    }
}

fn check_ns_id(
    x: &NsName,
    errors: &mut Vec<Error>,
    scope: &Vec<&str>,
    exports: &HashMap<String, &Exports>,
) {
    if x.namespace == "OS" {
        return;
    }
    if x.namespace != "" {
        let e = exports.get(&x.namespace).unwrap();
        for f in &e.fns {
            if f.form.name == x.name {
                return;
            }
        }
        if e.types.contains(&x.name) {
            return;
        }
        for c in &e.consts {
            if c.id.name == x.name {
                return;
            }
        }
        errors.push(Error {
            message: format!("{} doesn't have exported {}", x.namespace, x.name),
            pos: x.pos.clone(),
        });
    } else {
        if !scope.contains(&x.name.as_str()) {
            errors.push(Error {
                message: format!("unknown identifier: {}", x.name),
                pos: x.pos.clone(),
            });
        }
    }
}

fn check_id(x: &Identifier, errors: &mut Vec<Error>, scope: &Vec<&str>) {
    if !scope.contains(&x.name.as_str()) {
        errors.push(Error {
            message: format!("unknown identifier: {}", x.name),
            pos: x.pos.clone(),
        });
    }
}
