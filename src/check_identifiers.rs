use crate::nodes::*;

pub struct Error {
    pub message: String,
    pub pos: String,
}

pub fn run(m: &Module) -> Vec<Error> {
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
                check_body(&f.body, &mut messages, &function_scope);
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

fn check_body(body: &Body, errors: &mut Vec<Error>, known: &Vec<&str>) {
    let mut scope: Vec<&str> = known.clone();
    for s in &body.statements {
        match s {
            Statement::Expression(e) => {
                check_expr(e, errors, &scope);
            }
            Statement::For {
                init,
                condition,
                action,
                body,
            } => {
                let ss = &mut scope.clone();
                match init {
                    ForInit::LoopCounterDeclaration {
                        type_name,
                        form,
                        value,
                    } => {
                        ss.push(form.name.as_str());
                    }
                    ForInit::Expression(x) => {
                        check_expr(x, errors, ss);
                    }
                }
                check_body(body, errors, ss);
            }
            Statement::If {
                condition,
                body,
                else_body,
            } => {
                let ss = &scope.clone();
                check_expr(condition, errors, ss);
                check_body(body, errors, ss);
                if else_body.is_some() {
                    check_body(else_body.as_ref().unwrap(), errors, ss);
                }
            }
            Statement::Return { expression } => match expression {
                Some(x) => {
                    check_expr(x, errors, &scope);
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
                        check_expr(v.as_ref().unwrap(), errors, &scope);
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

fn check_expr(e: &Expression, errors: &mut Vec<Error>, scope: &Vec<&str>) {
    match e {
        Expression::ArrayIndex { array, index } => {
            check_expr(array, errors, scope);
            check_expr(index, errors, scope);
        }
        Expression::BinaryOp { op, a, b } => {
            if op != "->" && op != "." {
                check_expr(a, errors, scope);
                check_expr(b, errors, scope);
            }
        }
        Expression::Cast { type_name, operand } => {
            check_expr(operand, errors, scope);
        }
        Expression::CompositeLiteral(x) => {
            for e in &x.entries {
                match &e.key {
                    Some(x) => check_expr(x, errors, scope),
                    None => {}
                }
                check_expr(&e.value, errors, scope);
            }
        }
        Expression::FunctionCall {
            function,
            arguments,
        } => {
            check_expr(function, errors, scope);
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
            todo!();
        }
        Expression::PostfixOperator {
            operator: _,
            operand,
        } => {
            check_expr(operand, errors, scope);
        }
        Expression::PrefixOperator {
            operator: _,
            operand,
        } => {
            check_expr(operand, errors, scope);
        }
        Expression::Sizeof { argument } => {
            match argument.as_ref() {
                SizeofArgument::Typename(_) => {
                    //
                }
                SizeofArgument::Expression(x) => {
                    check_expr(&x, errors, scope);
                }
            }
        }
    }
}
