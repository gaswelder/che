use crate::{exports::Exports, nodes::*};
use std::collections::HashMap;

pub struct Error {
    pub message: String,
    pub pos: String,
}

pub fn run(m: &Module, exports: &HashMap<String, &Exports>) -> Vec<Error> {
    let mut messages: Vec<Error> = Vec::new();
    let mut scope = vec![
        "acos",
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
        "sin",
        "snprintf",
        "strcmp",
        "strcpy",
        "strlen",
        "time",
        "tmpfile",
        "true",
        "va_end",
        "va_start",
        "vfprintf",
        "vprintf",
        "vsnprintf",
    ];
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
            }
            _ => {}
        }
    }

    for e in &m.elements {
        match e {
            ModuleObject::Typedef(x) => {
                //
            }
            ModuleObject::Enum { is_pub, members } => {
                //
            }
            ModuleObject::FunctionDeclaration(f) => {
                let mut function_scope = scope.clone();
                for pl in &f.parameters.list {
                    for f in &pl.forms {
                        function_scope.push(&f.name);
                    }
                }
                check_body(&f.body, &mut messages, &function_scope, exports);
            }
            ModuleObject::Macro { name, value } => {
                if name == "known" {
                    scope.push(value.trim());
                }
            }
            ModuleObject::ModuleVariable {
                type_name,
                form,
                value,
            } => {
                //
            }
            ModuleObject::StructAliasTypedef {
                is_pub,
                struct_name,
                type_alias,
            } => {
                //
            }
            ModuleObject::StructTypedef(x) => {
                //
            }
        }
    }
    return messages;
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
                        loop_scope.push(form.name.as_str());
                    }
                    ForInit::Expression(x) => {
                        check_expr(x, errors, loop_scope, exports);
                    }
                }
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
                //
            }
            Statement::VariableDeclaration {
                type_name,
                forms,
                values,
            } => {
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
                //
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
            check_expr(operand, errors, scope, exports);
        }
        Expression::CompositeLiteral(x) => {
            for e in &x.entries {
                match &e.key {
                    Some(x) => check_expr(x, errors, scope, exports),
                    None => {}
                }
                check_expr(&e.value, errors, scope, exports);
            }
        }
        Expression::FunctionCall {
            function,
            arguments,
        } => {
            check_expr(function, errors, scope, exports);
        }
        Expression::Identifier(x) => {
            if !scope.contains(&x.name.as_str()) {
                errors.push(Error {
                    message: format!("unknown identifier: {}", x.name),
                    pos: x.pos.clone(),
                });
            }
        }
        Expression::Literal(x) => {
            //
        }
        Expression::NsName(x) => {
            if x.namespace != "" {
                let e = exports.get(&x.namespace).unwrap();
                for f in &e.fns {
                    if f.form.name == x.name {
                        return;
                    }
                }
                errors.push(Error {
                    message: format!("{} doesn't have exported {}", x.namespace, x.name),
                    pos: x.pos.clone(),
                });
                return;
            }
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
