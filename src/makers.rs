use crate::c;
use crate::format_che;
use crate::nodes;

//
// module elements
//
pub fn func_calloc() -> c::ModElem {
    let arg1 = c::CTypeForm {
        type_name: c::Typename {
            is_const: false,
            name: "size_t".to_string(),
        },
        form: c::Form {
            indexes: vec![],
            name: "count".to_string(),
            stars: "".to_string(),
        },
    };
    let arg2 = c::CTypeForm {
        type_name: c::Typename {
            is_const: false,
            name: "size_t".to_string(),
        },
        form: c::Form {
            indexes: vec![],
            name: "size".to_string(),
            stars: "".to_string(),
        },
    };

    // void *x = calloc(count, size)
    let xdecl = c::Statement::VarDecl {
        type_name: c::Typename {
            is_const: false,
            name: "void".to_string(),
        },
        forms: vec![c::Form {
            indexes: vec![],
            name: "x".to_string(),
            stars: "*".to_string(),
        }],
        values: vec![Some(expr_call(
            "calloc",
            vec![expr_id("count"), expr_id("size")],
        ))],
    };

    let printerror = c::Statement::Expression(expr_call(
        "fprintf",
        vec![expr_id("stderr"), expr_str("calloc failed")],
    ));

    let check = c::Statement::If {
        condition: expr_neg(expr_id("x")),
        body: c::Body {
            statements: vec![printerror, st_exit1()],
        },
        else_body: None,
    };

    c::ModElem::FuncDef(c::FuncDef {
        is_static: true,
        type_name: c::Typename {
            is_const: false,
            name: "void".to_string(),
        },
        form: c::Form {
            stars: "*".to_string(),
            name: "calloc_or_panic".to_string(),
            indexes: Vec::new(),
        },
        parameters: c::FuncParams {
            list: vec![arg1, arg2],
            variadic: false,
        },
        body: c::Body {
            statements: vec![
                xdecl,
                check,
                c::Statement::Return {
                    expression: Some(expr_id("x")),
                },
            ],
        },
    })
}

pub fn func_minmax_ii(f: &str) -> c::ModElem {
    let arg1 = c::CTypeForm {
        type_name: c::Typename {
            is_const: false,
            name: "int".to_string(),
        },
        form: c::Form {
            indexes: vec![],
            name: "a".to_string(),
            stars: "".to_string(),
        },
    };
    let arg2 = c::CTypeForm {
        type_name: c::Typename {
            is_const: false,
            name: "int".to_string(),
        },
        form: c::Form {
            indexes: vec![],
            name: "b".to_string(),
            stars: "".to_string(),
        },
    };

    //
    // if (a < b) return a;
    //
    let op = if f == "min" { "<" } else { ">" };
    c::ModElem::FuncDef(c::FuncDef {
        is_static: true,
        type_name: c::Typename {
            is_const: false,
            name: "int".to_string(),
        },
        form: c::Form {
            stars: "".to_string(),
            name: format!("__{}_ii", f),
            indexes: Vec::new(),
        },
        parameters: c::FuncParams {
            list: vec![arg1, arg2],
            variadic: false,
        },
        body: c::Body {
            statements: vec![
                c::Statement::If {
                    condition: expr_binop(expr_id("a"), op, expr_id("b")),
                    body: c::Body {
                        statements: vec![c::Statement::Return {
                            expression: Some(expr_id("a")),
                        }],
                    },
                    else_body: None,
                },
                c::Statement::Return {
                    expression: Some(expr_id("b")),
                },
            ],
        },
    })
}

//
// statement
//

pub fn st_calltrace(filepath: &str, x: &nodes::FuncDecl) -> c::Statement {
    let loc = format!("{}:{}", filepath, format_che::fmt_form(&x.form));
    st_call("puts", vec![expr_str(&loc)])
}

pub fn st_switch(val: c::Expr, cases: Vec<c::CSwitchCase>, def: Option<c::Body>) -> c::Statement {
    c::Statement::Switch(c::Switch {
        value: val,
        cases,
        default: def,
    })
}

fn st_call(func: &str, args: Vec<c::Expr>) -> c::Statement {
    c::Statement::Expression(expr_call(func, args))
}

fn st_exit1() -> c::Statement {
    let one = c::Expr::Literal(c::CLiteral::Number("1".to_string()));
    c::Statement::Expression(expr_call("exit", vec![one]))
}

pub fn st_panic(filepath: &str, pos: &str, xargs: Vec<c::Expr>) -> c::Statement {
    let panic_pos = format!("{}:{}", filepath, pos);
    let stderr = c::Expr::Ident(String::from("stderr"));
    let mut outargs = vec![stderr.clone()];
    for x in xargs {
        outargs.push(x)
    }
    c::Statement::Block {
        statements: vec![
            st_call(
                "fprintf",
                vec![
                    stderr.clone(),
                    expr_str("*** panic at %s ***\\n"),
                    expr_str(panic_pos.as_str()),
                ],
            ),
            st_call("fprintf", outargs),
            st_call("fprintf", vec![stderr.clone(), expr_str("\\n")]),
            st_exit1(),
        ],
    }
}

//
// expr
//
pub fn expr_array(items: Vec<c::Expr>) -> c::Expr {
    let mut strentries = Vec::new();
    for val in items {
        strentries.push(c::CCompositeLiteralEntry {
            is_index: false,
            key: None,
            val,
        })
    }
    c::Expr::CompositeLiteral(c::CCompositeLiteral {
        entries: strentries,
    })
}

pub fn expr_str(s: &str) -> c::Expr {
    c::Expr::Literal(c::CLiteral::String(vec![String::from(s)]))
}

pub fn expr_num(val: &str) -> c::Expr {
    c::Expr::Literal(c::CLiteral::Number(val.to_string()))
}

pub fn expr_call(func: &str, args: Vec<c::Expr>) -> c::Expr {
    c::Expr::Call {
        func: Box::new(c::Expr::Ident(String::from(func))),
        args,
    }
}

pub fn expr_binop(a: c::Expr, op: &str, b: c::Expr) -> c::Expr {
    c::Expr::BinaryOp(c::BinaryOp {
        op: op.to_string(),
        a: Box::new(a),
        b: Box::new(b),
    })
}

pub fn expr_id(n: &str) -> c::Expr {
    c::Expr::Ident(n.to_string())
}

pub fn expr_neg(operand: c::Expr) -> c::Expr {
    c::Expr::PrefixOp {
        operator: String::from("!"),
        operand: Box::new(operand),
    }
}
