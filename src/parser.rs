use crate::buf::Pos;
use crate::errors::{Error, TWithErrors};
use crate::lexer::{Lexer, Token};
use crate::nodes;
use crate::preparser::ModuleInfo;
use crate::{cspec, nodes::*};

#[derive(Clone, Debug)]
pub struct ParseCtx {
    pub thismod: ModuleInfo,
    pub allmods: Vec<ModuleInfo>,
}

impl ParseCtx {
    pub fn is_imported_ns(&self, ns: &String) -> bool {
        if ns == "OS" {
            return true;
        }
        for x in &self.thismod.imports {
            if x.ns == *ns {
                return true;
            }
        }
        return false;
    }
}

pub fn parse_module(l: &mut Lexer, ctx: &ParseCtx) -> Result<Module, Vec<Error>> {
    let mut module_objects: Vec<ModElem> = vec![];
    let mut errors = Vec::new();
    while l.more() {
        match l.peek().unwrap().kind.as_str() {
            "import" => {
                // The context already contains all parsed and resolved imports,
                // here this node is added only for completeness, to allow the
                // source code checkers look at them.
                l.get().unwrap();
                // module_objects.push(ModElem::Import(ImportNode {
                //     specified_path: t.content.clone(),
                //     pos: t.pos.clone(),
                // }))
            }
            "macro" => match parse_compat_macro(l) {
                Ok(r) => module_objects.push(r),
                Err(e) => {
                    errors.push(e);
                    break;
                }
            },
            _ => match parse_module_object(l, ctx) {
                Ok(r) => {
                    for e in r.errors {
                        errors.push(e);
                    }
                    module_objects.push(r.obj);
                }
                Err(e) => {
                    errors.push(e);
                    break;
                }
            },
        }
    }
    if errors.len() > 0 {
        return Err(errors);
    }
    let exports = get_exports(&module_objects);
    Ok(Module {
        elements: module_objects,
        exports,
    })
}

fn get_exports(elems: &Vec<ModElem>) -> Exports {
    let mut exports = Exports {
        consts: Vec::new(),
        fns: Vec::new(),
        types: Vec::new(),
        structs: Vec::new(),
    };
    for e in elems {
        match e {
            ModElem::Enum(x) => {
                if x.is_pub {
                    for m in &x.entries {
                        exports.consts.push(m.clone());
                    }
                }
            }
            ModElem::Typedef(x) => {
                if x.ispub {
                    exports.types.push(x.alias.clone());
                }
            }
            ModElem::StructTypedef(x) => {
                if x.ispub {
                    exports.types.push(x.name.clone());
                    exports.structs.push(x.clone());
                }
            }
            // ModuleObject::StructAliasTypedef {
            //     is_pub,
            //     struct_name: _,
            //     type_alias,
            // } => {
            //     if *is_pub {
            //         exports.structs.push(StructTypedef {
            //             fields: Vec::new(),
            //             is_pub: *is_pub,
            //             name: type_alias.clone(),
            //         })
            //     }
            // }
            ModElem::FuncDecl(f) => {
                if f.ispub {
                    exports.fns.push(f.clone())
                }
            }
            _ => {}
        }
    }
    return exports;
}

fn parse_compat_macro(l: &mut Lexer) -> Result<ModElem, Error> {
    let tok = expect(l, "macro", None)?;
    let content = tok.content.clone();
    let pos = content.find(" ");
    if pos.is_none() {
        return Err(Error {
            message: format!("can't get macro name from '{}'", content),
            pos: tok.pos,
        });
    }
    let (name, value) = content.split_at(pos.unwrap());

    return Ok(ModElem::Macro(nodes::Macro {
        name: name[1..].to_string(),
        value: value.to_string(),
        pos: tok.pos.clone(),
    }));
}

fn parse_module_object(l: &mut Lexer, ctx: &ParseCtx) -> Result<TWithErrors<ModElem>, Error> {
    let mut is_pub = false;
    let pos = l.peek().unwrap().pos.clone();
    if l.follows("pub") {
        l.get();
        is_pub = true;
    }
    if l.follows("enum") {
        return Ok(TWithErrors {
            obj: parse_enum(l, is_pub, ctx)?,
            errors: Vec::new(),
        });
    }
    if l.follows("typedef") {
        return Ok(TWithErrors {
            obj: parse_typedef(is_pub, l, ctx)?,
            errors: Vec::new(),
        });
    }
    let type_name = parse_typename(l, ctx)?;
    let form = parse_form(l, ctx)?;
    if l.peek().unwrap().kind == "(" {
        let r = parse_function_declaration(l, is_pub, type_name, form, ctx, pos)?;
        return Ok(TWithErrors {
            obj: r.obj,
            errors: r.errors,
        });
    }

    expect(l, "=", Some("module variable"))?;
    if is_pub {
        return Err(Error {
            message: "module variables can't be exported".to_string(),
            pos,
        });
    }
    let value = parse_expr(l, 0, ctx)?;
    expect(l, ";", Some("module variable declaration"))?;
    return Ok(TWithErrors {
        obj: ModElem::ModVar(VarDecl {
            typename: type_name,
            form,
            value: Some(value),
            pos,
        }),
        errors: Vec::new(),
    });
}

fn parse_expr(l: &mut Lexer, strength: usize, ctx: &ParseCtx) -> Result<Expr, Error> {
    let prefix_strength = operator_strength("prefix");
    if strength < prefix_strength {
        return parse_seq_expr(l, strength, ctx);
    }
    if strength == prefix_strength {
        return parse_prefix_expr(l, ctx);
    }
    if strength == operator_strength("suffix") {
        return base(l, ctx);
    }
    panic!("unexpected n: {}", strength)
}

fn parse_seq_expr(l: &mut Lexer, strength: usize, ctx: &ParseCtx) -> Result<Expr, Error> {
    let mut r = parse_expr(l, strength + 1, ctx)?;
    while l.more() {
        // Continue if next is an operator with the same strength.
        let next = &l.peek().unwrap().kind;
        if !is_op(&next) || operator_strength(&next) != strength {
            break;
        }
        let tok = l.get().unwrap();
        let op = tok.kind;
        let r2 = parse_expr(l, strength + 1, ctx)?;
        r = Expr::BinaryOp(nodes::BinaryOp {
            pos: tok.pos,
            op,
            a: Box::new(r),
            b: Box::new(r2),
        })
    }
    return Ok(r);
}

fn parse_prefix_expr(l: &mut Lexer, ctx: &ParseCtx) -> Result<Expr, Error> {
    let token = l.get().unwrap();

    // *{we are here}*foo
    if is_prefix_op(&token.kind) {
        let r = parse_prefix_expr(l, ctx)?;
        return Ok(Expr::PrefixOperator(nodes::PrefixOp {
            pos: token.pos,
            operator: token.kind,
            operand: Box::new(r),
        }));
    }

    let next = String::from(&token.kind);
    // cast
    // * (foo_t)x
    if next == "("
        && l.peek().unwrap().kind == "word"
        && is_type(l.peek().unwrap().content.as_str(), &ctx.thismod.typedefs)
    {
        let typeform = parse_bare_typeform(l, ctx)?;
        expect(l, ")", Some("typecast"))?;
        return Ok(Expr::Cast(nodes::Cast {
            typeform,
            operand: Box::new(parse_prefix_expr(l, ctx)?),
        }));
    }

    if token.kind == "sizeof" {
        l.unget(token);
        return parse_sizeof(l, ctx);
    }

    l.unget(token);
    return base(l, ctx);
}

fn base(l: &mut Lexer, ctx: &ParseCtx) -> Result<Expr, Error> {
    // println!("      base(): {:?}", l.peek());
    let mut r = read_expression_atom(l, ctx)?;
    while l.more() {
        let next = l.peek().unwrap();
        // call
        if next.kind == "(" {
            r = parse_call(l, ctx, r)?;
            continue;
        }

        // index
        if next.kind == "[" {
            expect(l, "[", Some("array index"))?;
            let index = parse_expr(l, 0, ctx)?;
            expect(l, "]", Some("array index"))?;
            r = Expr::ArrIndex(nodes::ArrayIndex {
                array: Box::new(r),
                index: Box::new(index),
            });
            continue;
        }

        if is_postfix_op(&next.kind) {
            r = Expr::PostfixOperator(nodes::PostfixOp {
                operator: l.get().unwrap().kind,
                operand: Box::new(r),
            });
            continue;
        }

        if next.kind == "->" || next.kind == "." {
            let optok = l.get().unwrap();
            let tok = expect(l, "word", Some("field access field name - identifier"))?;
            r = Expr::FieldAccess(nodes::FieldAccess {
                pos: optok.pos,
                op: optok.kind,
                target: Box::new(r),
                field_name: tok.content,
            });
            continue;
        }

        break;
    }
    return Ok(r);
}

fn type_follows(l: &mut Lexer, ctx: &ParseCtx) -> bool {
    if !l.peek().is_some() {
        return false;
    }
    if l.peek().unwrap().kind == "const" {
        return true;
    }
    if l.peek().unwrap().kind != "word" {
        return false;
    }
    // <mod>.<name> where <mod> is any imported module that has exported <name>?
    match l.peekn(3) {
        Some(three) => {
            if three[1].kind == "." && three[2].kind == "word" {
                let nsname = &three[0].content;
                let typename = &three[2].content;

                let import = ctx
                    .thismod
                    .imports
                    .iter()
                    .find(|x| x.ns == *nsname)
                    .and_then(|imp| ctx.allmods.iter().find(|x| x.filepath == imp.path));
                match import {
                    Some(imp) => {
                        if imp.typedefs.contains(typename) {
                            return true;
                        }
                    }
                    None => {}
                }
            }
        }
        None => {}
    }
    // any of locally defined types?
    return is_type(&l.peek().unwrap().content, &ctx.thismod.typedefs);
}

fn ns_follows(l: &mut Lexer, ctx: &ParseCtx) -> bool {
    if !l.more() {
        return false;
    }
    let t = l.peek().unwrap();
    if t.kind != "word" {
        return false;
    }
    return ctx.is_imported_ns(&l.peek().unwrap().content);
}

// foo.bar where foo is a module
// or just bar
fn read_ns_id(l: &mut Lexer, ctx: &ParseCtx) -> Result<NsName, Error> {
    let a = expect(l, "word", Some("nsid"))?;
    let name = &a.content;
    if ctx.is_imported_ns(name) {
        match l.peekn(2) {
            Some(two) => {
                if two[0].kind == "." && two[1].kind == "word" {
                    l.get();
                    let b = l.get().unwrap();
                    return Ok(NsName {
                        ns: a.content,
                        name: b.content,
                        pos: a.pos,
                    });
                }
            }
            None => {}
        }
    }
    return Ok(NsName {
        ns: String::new(),
        name: a.content,
        pos: a.pos,
    });
}

fn read_expression_atom(l: &mut Lexer, ctx: &ParseCtx) -> Result<Expr, Error> {
    if !l.more() {
        return Err(Error {
            message: String::from("id: unexpected end of input"),
            pos: l.pos(),
        });
    }
    if ns_follows(l, ctx) {
        return Ok(Expr::NsName(read_ns_id(l, ctx)?));
    }

    let next = l.get().unwrap();
    return match next.kind.as_str() {
        "word" => Ok(Expr::NsName(NsName {
            pos: next.pos,
            ns: String::from(""),
            name: next.content,
        })),
        // "word" => Ok(Expr::Ident(Ident {
        //     name: next.content,
        //     pos: next.pos,
        // })),
        "num" | "string" | "char" => {
            l.unget(next);
            Ok(Expr::Literal(parse_literal(l)?))
        }
        "(" => {
            let e = parse_expr(l, 0, ctx);
            expect(l, ")", Some("parenthesized expression"))?;
            e
        }
        "{" => {
            l.unget(next);
            Ok(Expr::CompositeLiteral(parse_composite_literal(l, ctx)?))
        }
        _ => Err(Error {
            message: format!("id: unexpected token {}", next),
            pos: next.pos,
        }),
    };
}

fn is_op(token_type: &str) -> bool {
    let ops = [
        "+", "-", "*", "/", "=", "|", "&", "~", "^", "<", ">", "%", "+=", "-=", "*=", "/=", "%=",
        "&=", "^=", "|=", "++", "--", "->", ".", ">>", "<<", "<=", ">=", "&&", "||", "==", "!=",
        "<<=", ">>=",
    ];
    return ops.contains(&token_type);
}

pub fn operator_strength(op: &str) -> usize {
    // The lower the operator, the stronger it binds. So, for example,
    // a = 1, b = 2 will be parsed as (a = 1), (b = 2).
    let map = [
        /* 1 */ vec![","],
        /* 2 */
        vec![
            "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=",
        ],
        /* 3 */ vec!["||"],
        /* 4 */ vec!["&&"],
        /* 5 */ vec!["|"],
        /* 6 */ vec!["^"],
        /* 7 */ vec!["&"],
        /* 8 */ vec!["!=", "=="],
        /* 9 */ vec![">", "<", ">=", "<="],
        /* 10 */ vec!["<<", ">>"],
        /* 11 */ vec!["+", "-"],
        /* 12 */ vec!["*", "/", "%"],
        /* 13 */ vec!["prefix", "unary", "cast", "deref", "sizeof", "address"],
        /* 14 */ vec!["->", ".", "index", "call", "suffix"],
    ];
    for (i, ops) in map.iter().enumerate() {
        if ops.contains(&op) {
            return i + 1;
        }
    }
    panic!("unknown operator: '{}'", op);
}

fn is_postfix_op(token: &str) -> bool {
    let ops = [
        "++", "--", // "(", "["
    ];
    return ops.to_vec().contains(&token);
}

fn is_prefix_op(op: &str) -> bool {
    let ops = ["!", "--", "++", "*", "~", "&", "-"];
    return ops.to_vec().contains(&op);
}

fn is_type(name: &str, typenames: &Vec<String>) -> bool {
    let keywords = ["struct", "enum", "union"];
    cspec::has_type(name)
        || keywords.to_vec().contains(&name)
        || typenames.to_vec().contains(&String::from(name))
}

fn with_comment(comment: Option<&str>, message: String) -> String {
    match comment {
        Some(s) => format!("{}: {}", s, message),
        None => message.to_string(),
    }
}

fn expect(l: &mut Lexer, kind: &str, comment: Option<&str>) -> Result<Token, Error> {
    if !l.more() {
        return Err(Error {
            message: with_comment(comment, format!("expected '{}', got end of file", kind)),
            pos: l.pos(),
        });
    }
    let next = l.peek().unwrap();
    if next.kind != kind {
        return Err(Error {
            message: with_comment(
                comment,
                format!("expected '{}', got '{}'", kind, token_to_string(next),),
            ),
            pos: next.pos.clone(),
        });
    }
    return Ok(l.get().unwrap());
}

fn parse_typename(l: &mut Lexer, ctx: &ParseCtx) -> Result<Typename, Error> {
    let is_const = l.eat("const");
    let name = read_ns_id(l, ctx)?;
    return Ok(Typename { is_const, name });
}

fn parse_bare_typeform(l: &mut Lexer, ctx: &ParseCtx) -> Result<BareTypeform, Error> {
    let type_name = parse_typename(l, ctx)?;
    let mut hops = 0;
    while l.follows("*") {
        hops += 1;
        l.get();
    }
    return Ok(BareTypeform {
        typename: type_name,
        hops,
    });
}

fn parse_anonymous_parameters(l: &mut Lexer, ctx: &ParseCtx) -> Result<AnonymousParameters, Error> {
    let mut params = AnonymousParameters {
        ellipsis: false,
        forms: Vec::new(),
    };
    expect(l, "(", Some("anonymous function parameters"))?;
    if !l.follows(")") {
        params.forms.push(parse_bare_typeform(l, ctx)?);
        while l.follows(",") {
            l.get();
            if l.follows("...") {
                l.get();
                params.ellipsis = true;
                break;
            }
            params.forms.push(parse_bare_typeform(l, ctx)?);
        }
    }
    expect(l, ")", Some("anonymous function parameters"))?;
    return Ok(params);
}

fn parse_literal(l: &mut Lexer) -> Result<Literal, Error> {
    let next = l.peek().unwrap();
    if next.kind == "string".to_string() {
        let value = l.get().unwrap().content;
        return Ok(Literal::String(value));
    }
    if next.kind == "num".to_string() {
        let value = l.get().unwrap().content;
        return Ok(Literal::Number(value));
    }
    if next.kind == "char".to_string() {
        let value = l.get().unwrap().content;
        return Ok(Literal::Char(value));
    }
    if next.kind == "word" && next.content == "NULL" {
        l.get();
        return Ok(Literal::Null);
    }
    return Err(Error {
        message: format!("literal expected, got {}", l.peek().unwrap()),
        pos: l.peek().unwrap().pos.clone(),
    });
}

fn parse_enum(l: &mut Lexer, is_pub: bool, ctx: &ParseCtx) -> Result<ModElem, Error> {
    let pos = l.peek().unwrap().pos.clone();
    let mut entries: Vec<EnumEntry> = Vec::new();
    expect(l, "enum", Some("enum definition"))?;
    expect(l, "{", Some("enum definition"))?;
    loop {
        let name = expect(l, "word", Some("enum entry - identifier"))?;
        let val = if l.eat("=") {
            Some(parse_expr(l, 0, ctx)?)
        } else {
            None
        };
        entries.push(EnumEntry {
            name: name.content,
            val,
        });
        if !l.eat(",") {
            break;
        }
        if l.follows("}") {
            break;
        }
    }
    expect(l, "}", Some("enum definition"))?;
    l.eat(";");
    return Ok(ModElem::Enum(nodes::Enum {
        is_pub,
        entries,
        pos,
    }));
}

fn parse_composite_literal(l: &mut Lexer, ctx: &ParseCtx) -> Result<CompLiteral, Error> {
    let mut result = CompLiteral {
        entries: Vec::new(),
    };
    expect(l, "{", Some("composite literal"))?;
    loop {
        if l.peek().unwrap().kind == "}" {
            break;
        }
        result.entries.push(parse_composite_literal_entry(l, ctx)?);
        if l.peek().unwrap().kind == "," {
            l.get();
        } else {
            break;
        }
    }
    expect(l, "}", Some("struct literal"))?;
    return Ok(result);
}

fn parse_composite_literal_entry(
    l: &mut Lexer,
    ctx: &ParseCtx,
) -> Result<CompositeLiteralEntry, Error> {
    if l.peek().unwrap().kind == "." {
        expect(l, ".", Some("struct literal member"))?;
        let tok = expect(l, "word", Some("struct literal entry - identifier"))?;
        expect(l, "=", Some("struct literal member"))?;
        let value = parse_expr(l, 0, ctx)?;
        return Ok(CompositeLiteralEntry {
            is_index: false,
            key: Some(Expr::NsName(NsName {
                pos: tok.pos,
                ns: String::from(""),
                name: tok.content,
            })),
            value,
        });
    }
    if l.follows("[") {
        l.get();
        let key = parse_expr(l, 0, ctx)?;
        expect(l, "]", Some("array literal index"))?;
        expect(l, "=", Some("array literal index"))?;
        let value = parse_expr(l, 0, ctx)?;
        return Ok(CompositeLiteralEntry {
            is_index: true,
            key: Some(key),
            value,
        });
    }
    return Ok(CompositeLiteralEntry {
        is_index: false,
        key: None,
        value: parse_expr(l, 0, ctx)?,
    });
}

fn parse_sizeof(l: &mut Lexer, ctx: &ParseCtx) -> Result<Expr, Error> {
    expect(l, "sizeof", None)?;
    expect(l, "(", None)?;
    let argument = if type_follows(l, ctx) {
        SizeofArg::Typename(parse_typename(l, ctx)?)
    } else {
        SizeofArg::Expr(parse_expr(l, 0, ctx)?)
    };
    expect(l, ")", None)?;
    return Ok(Expr::Sizeof(nodes::Sizeof {
        arg: Box::new(argument),
    }));
}

fn parse_call(l: &mut Lexer, ctx: &ParseCtx, func: Expr) -> Result<Expr, Error> {
    let mut args = Vec::new();
    let tok = expect(l, "(", None)?;
    if l.more() && l.peek().unwrap().kind != ")" {
        args.push(parse_expr(l, 0, ctx)?);
        while l.follows(",") {
            l.get();
            args.push(parse_expr(l, 0, ctx)?);
        }
    }
    expect(l, ")", None)?;
    return Ok(Expr::Call(nodes::Call {
        pos: tok.pos,
        func: Box::new(func),
        args,
    }));
}

fn parse_while(l: &mut Lexer, ctx: &ParseCtx) -> Result<TWithErrors<Statement>, Error> {
    expect(l, "while", None)?;
    expect(l, "(", None)?;
    let condition = parse_expr(l, 0, ctx)?;
    expect(l, ")", None)?;
    let body = parse_statements_block(l, ctx)?;
    return Ok(TWithErrors {
        obj: Statement::While(nodes::While {
            cond: condition,
            body: body.obj,
        }),
        errors: body.errors,
    });
}

fn parse_statement(l: &mut Lexer, ctx: &ParseCtx) -> Result<TWithErrors<Statement>, Error> {
    if !l.more() {
        return Err(Error {
            message: String::from("reached end of file while parsing statement"),
            pos: l.pos(),
        });
    }
    if type_follows(l, ctx) {
        return Ok(TWithErrors {
            obj: parse_variable_declaration(l, ctx)?,
            errors: Vec::new(),
        });
    }

    let next = l.peek().unwrap();
    match next.kind.as_str() {
        "break" => {
            l.get().unwrap();
            expect(l, ";", Some("break statement"))?;
            Ok(TWithErrors {
                obj: Statement::Break,
                errors: Vec::new(),
            })
        }
        "continue" => {
            l.get().unwrap();
            expect(l, ";", Some("continue statement"))?;
            Ok(TWithErrors {
                obj: Statement::Continue,
                errors: Vec::new(),
            })
        }
        "for" => parse_for(l, ctx),
        "if" => parse_if(l, ctx),
        "return" => Ok(TWithErrors {
            obj: parse_return(l, ctx)?,
            errors: Vec::new(),
        }),
        "switch" => read_switch(l, ctx),
        "while" => parse_while(l, ctx),
        _ => {
            let expr = parse_expr(l, 0, ctx)?;
            expect(l, ";", Some("parsing statement"))?;
            return Ok(TWithErrors {
                obj: Statement::Expression(expr),
                errors: Vec::new(),
            });
        }
    }
}

fn parse_variable_declaration(l: &mut Lexer, ctx: &ParseCtx) -> Result<Statement, Error> {
    let pos = l.peek().unwrap().pos.clone();
    let type_name = parse_typename(l, ctx)?;
    let form = parse_form(l, ctx)?;
    let value = if l.eat("=") {
        Some(parse_expr(l, 0, ctx)?)
    } else {
        None
    };
    expect(l, ";", None)?;
    return Ok(Statement::VarDecl(VarDecl {
        pos,
        typename: type_name,
        form,
        value,
    }));
}

fn parse_return(l: &mut Lexer, ctx: &ParseCtx) -> Result<Statement, Error> {
    expect(l, "return", None)?;
    if l.peek().unwrap().kind == ";" {
        l.get();
        return Ok(Statement::Return(nodes::Return { expression: None }));
    }
    let expression = parse_expr(l, 0, ctx)?;
    expect(l, ";", None)?;
    return Ok(Statement::Return(nodes::Return {
        expression: Some(expression),
    }));
}

fn parse_form(l: &mut Lexer, ctx: &ParseCtx) -> Result<Form, Error> {
    // *argv[]
    // linechars[]
    // buf[SIZE * 2]
    let mut node = Form {
        hops: 0,
        name: String::new(),
        indexes: vec![],
        pos: l.peek().unwrap().pos.clone(),
    };

    while l.follows("*") {
        l.get().unwrap();
        node.hops += 1;
    }

    let tok = expect(l, "word", Some("form"))?;
    if tok.content == "" {
        return Err(Error {
            message: String::from("missing word content"),
            pos: tok.pos,
        });
    }
    node.name = tok.content.clone();

    while l.follows("[") {
        l.get().unwrap();
        let e: Option<Expr>;
        if l.more() && l.peek().unwrap().kind != "]" {
            e = Some(parse_expr(l, 0, ctx)?);
        } else {
            e = None;
        }
        node.indexes.push(e);
        expect(l, "]", None)?;
    }
    return Ok(node);
}

fn parse_if(lexer: &mut Lexer, ctx: &ParseCtx) -> Result<TWithErrors<Statement>, Error> {
    let condition;
    let body;
    let mut else_body = None;
    let mut errors = Vec::new();

    expect(lexer, "if", Some("if statement"))?;
    expect(lexer, "(", Some("if statement"))?;
    condition = parse_expr(lexer, 0, ctx)?;
    expect(lexer, ")", Some("if statement"))?;
    body = parse_statements_block(lexer, ctx)?;
    for e in body.errors {
        errors.push(e)
    }
    if lexer.follows("else") {
        lexer.get();
        let r = parse_statements_block(lexer, ctx)?;
        for e in r.errors {
            errors.push(e)
        }
        else_body = Some(r.obj);
    }
    return Ok(TWithErrors {
        obj: Statement::If(nodes::If {
            condition,
            body: body.obj,
            else_body,
        }),
        errors,
    });
}

fn parse_for(l: &mut Lexer, ctx: &ParseCtx) -> Result<TWithErrors<Statement>, Error> {
    expect(l, "for", None)?;
    expect(l, "(", None)?;

    let init: Option<ForInit>;
    if l.follows(";") {
        init = None;
    } else if l.peek().unwrap().kind == "word"
        && is_type(&l.peek().unwrap().content, &ctx.thismod.typedefs)
    {
        let type_name = parse_typename(l, ctx)?;
        let form = parse_form(l, ctx)?;
        expect(l, "=", None)?;
        let value = parse_expr(l, 0, ctx)?;
        init = Some(ForInit::DeclLoopCounter {
            type_name,
            form,
            value,
        });
    } else {
        init = Some(ForInit::Expr(parse_expr(l, 0, ctx)?));
    }
    expect(l, ";", Some("for loop initializer"))?;

    let condition = if l.follows(";") {
        None
    } else {
        Some(parse_expr(l, 0, ctx)?)
    };
    expect(l, ";", Some("for loop condition"))?;

    let action = if l.follows(")") {
        None
    } else {
        Some(parse_expr(l, 0, ctx)?)
    };
    expect(l, ")", Some("for loop postaction"))?;

    let body = parse_statements_block(l, ctx)?;

    return Ok(TWithErrors {
        obj: Statement::For(nodes::For {
            init,
            condition,
            action,
            body: body.obj,
        }),
        errors: body.errors,
    });
}

fn read_switch(l: &mut Lexer, ctx: &ParseCtx) -> Result<TWithErrors<Statement>, Error> {
    expect(l, "switch", None)?;
    let mut is_str = false;
    match l.peek() {
        Some(t) => {
            if t.kind == "word" && t.content == "str" {
                is_str = true;
                l.get();
            }
        }
        None => {}
    };
    expect(l, "(", None)?;
    let value = parse_expr(l, 0, ctx)?;
    let mut cases: Vec<SwitchCase> = vec![];
    expect(l, ")", None)?;
    expect(l, "{", None)?;
    let mut errors = Vec::new();
    while l.follows("case") {
        let r = read_switch_case(l, ctx)?;
        for e in r.errors {
            errors.push(e);
        }
        cases.push(r.obj);
    }
    let mut default: Option<Body> = None;
    if l.follows("default") {
        expect(l, "default", None)?;
        expect(l, ":", None)?;
        let r = read_body(l, ctx)?;
        for e in r.errors {
            errors.push(e);
        }
        default = Some(r.obj);
    }
    expect(l, "}", None)?;
    return Ok(TWithErrors {
        obj: Statement::Switch(nodes::Switch {
            is_str,
            value,
            cases,
            default_case: default,
        }),
        errors,
    });
}

fn read_switch_case(l: &mut Lexer, ctx: &ParseCtx) -> Result<TWithErrors<SwitchCase>, Error> {
    expect(l, "case", None)?;
    let mut values = Vec::new();
    loop {
        let case_value = if l.follows("word") {
            SwitchCaseValue::Ident(read_ns_id(l, ctx)?)
        } else {
            SwitchCaseValue::Literal(parse_literal(l)?)
        };
        values.push(case_value);
        if l.follows(",") {
            l.get().unwrap();
        } else {
            break;
        }
    }
    expect(l, ":", None)?;
    let body = read_body(l, ctx)?;
    return Ok(TWithErrors {
        obj: SwitchCase {
            values,
            body: body.obj,
        },
        errors: body.errors,
    });
}

fn parse_function_declaration(
    l: &mut Lexer,
    is_pub: bool,
    type_name: Typename,
    form: Form,
    ctx: &ParseCtx,
    pos: Pos,
) -> Result<TWithErrors<ModElem>, Error> {
    // void cuespl {WE ARE HERE} (cue_t *c, mp3file *m) {...}

    let mut parameters: Vec<TypeAndForms> = vec![];
    let mut variadic = false;

    // (const int a,b, float c, ...)
    expect(l, "(", None)?;
    while type_follows(l, ctx) {
        parameters.push(parse_function_parameter(l, ctx)?);
        if !l.eat(",") {
            break;
        }
    }
    if l.follows("...") {
        l.get();
        variadic = true;
    }
    expect(l, ")", Some("parse_function_declaration"))?;

    let body = parse_statements_block(l, ctx)?;
    return Ok(TWithErrors {
        obj: ModElem::FuncDecl(FuncDecl {
            ispub: is_pub,
            typename: type_name,
            form,
            params: FuncParams {
                list: parameters,
                variadic,
            },
            body: body.obj,
            pos,
        }),
        errors: body.errors,
    });
}

fn parse_function_parameter(l: &mut Lexer, ctx: &ParseCtx) -> Result<TypeAndForms, Error> {
    // int a,b
    // const char *s
    // let pos = l.peek().unwrap().pos.clone();
    let type_name = parse_typename(l, ctx)?;
    let mut forms: Vec<Form> = Vec::new();
    forms.push(parse_form(l, ctx)?);
    while l.follows(",") {
        let t = l.get().unwrap();
        if type_follows(l, ctx) || l.follows("...") {
            l.unget(t);
            break;
        }
        forms.push(parse_form(l, ctx)?);
    }
    return Ok(TypeAndForms {
        typename: type_name,
        forms,
        // pos,
    });
}

fn parse_statements_block(l: &mut Lexer, ctx: &ParseCtx) -> Result<TWithErrors<Body>, Error> {
    if l.follows("{") {
        return read_body(l, ctx);
    }
    let s = parse_statement(l, ctx)?;
    return Ok(TWithErrors {
        obj: Body {
            statements: vec![s.obj],
        },
        errors: s.errors,
    });
}

fn read_body(l: &mut Lexer, ctx: &ParseCtx) -> Result<TWithErrors<Body>, Error> {
    let mut statements: Vec<Statement> = Vec::new();
    let mut errors = Vec::new();
    expect(l, "{", None)?;
    while !l.follows("}") {
        match parse_statement(l, ctx) {
            Ok(s) => {
                statements.push(s.obj);
                for e in s.errors {
                    errors.push(e)
                }
            }
            Err(e) => {
                while l.more() && !l.follows("}") && !l.follows(";") {
                    l.get().unwrap();
                }
                if l.follows(";") {
                    l.get().unwrap();
                }
                errors.push(e);
            }
        }
    }
    expect(l, "}", None)?;
    return Ok(TWithErrors {
        obj: Body { statements },
        errors,
    });
}

fn parse_union(l: &mut Lexer, ctx: &ParseCtx) -> Result<Union, Error> {
    let mut fields: Vec<UnionField> = vec![];
    expect(l, "union", None)?;
    expect(l, "{", None)?;
    while !l.follows("}") {
        let type_name = parse_typename(l, ctx)?;
        let form = parse_form(l, ctx)?;
        expect(l, ";", None)?;
        fields.push(UnionField { type_name, form });
    }
    expect(l, "}", None)?;
    let form = parse_form(l, ctx)?;
    expect(l, ";", None)?;
    return Ok(Union { form, fields });
}

fn parse_typedef(is_pub: bool, l: &mut Lexer, ctx: &ParseCtx) -> Result<ModElem, Error> {
    expect(l, "typedef", None)?;

    // typedef {int hour, minute, second} time_t;
    if l.follows("{") {
        // struct body
        let mut fields: Vec<StructEntry> = Vec::new();
        expect(l, "{", Some("struct type definition"))?;
        while l.more() {
            match l.peek().unwrap().kind.as_str() {
                "}" => break,
                "union" => fields.push(StructEntry::Union(parse_union(l, ctx)?)),
                _ => fields.push(StructEntry::Plain(parse_type_and_forms(l, ctx)?)),
            }
        }
        expect(l, "}", None)?;

        // type name
        let tok = expect(l, "word", Some("typedef typename - identifier"))?;
        expect(l, ";", Some("typedef"))?;
        return Ok(ModElem::StructTypedef(StructTypedef {
            ispub: is_pub,
            entries: fields,
            name: tok.content,
        }));
    }

    // typedef struct foo foo_t;
    if l.eat("struct") {
        let struct_name = expect(l, "word", Some("struct name"))?.content;
        let type_alias = expect(l, "word", Some("struct type alias"))?.content;
        expect(l, ";", Some("typedef"))?;
        return Ok(ModElem::StructAlias(nodes::StructAlias {
            ispub: is_pub,
            structname: struct_name,
            typename: type_alias,
        }));
    }

    // typedef void *thr_func(void *)
    // typedef foo bar;
    // typedef uint32_t md5sum_t[4];
    let typename = parse_typename(l, ctx)?;
    let mut stars: usize = 0;
    while l.follows("*") {
        l.get();
        stars += 1;
    }
    let tok = expect(l, "word", Some("typedef alias - identifier"))?;
    let params = if l.follows("(") {
        Some(parse_anonymous_parameters(l, ctx)?)
    } else {
        None
    };
    let mut size: usize = 0;
    if l.follows("[") {
        l.get();
        let num = expect(l, "num", None)?;
        size = num.content.parse().unwrap();
        expect(l, "]", None)?;
    }
    expect(l, ";", Some("typedef"))?;
    return Ok(ModElem::Typedef(Typedef {
        ispub: is_pub,
        type_name: typename,
        dereference_count: stars,
        function_parameters: params,
        array_size: size,
        alias: tok.content,
    }));
}

fn parse_type_and_forms(l: &mut Lexer, ctx: &ParseCtx) -> Result<TypeAndForms, Error> {
    if l.follows("struct") {
        return Err(Error {
            message: "Unexpected struct".to_string(),
            pos: l.peek().unwrap().pos.clone(),
        });
    }

    // let pos = l.peek().unwrap().pos.clone();
    let type_name = parse_typename(l, ctx)?;

    let mut forms: Vec<Form> = vec![];
    forms.push(parse_form(l, ctx)?);
    while l.follows(",") {
        l.get();
        forms.push(parse_form(l, ctx)?);
    }

    expect(l, ";", None)?;
    return Ok(TypeAndForms {
        typename: type_name,
        forms,
        // pos,
    });
}

fn token_to_string(token: &Token) -> String {
    if token.content == "" {
        return format!("[{}]", token.kind);
    }

    let n = 40;
    let mut c: String;
    if token.content.len() > n {
        c = token.content[0..(n - 3)].to_string() + "...";
    } else {
        c = token.content.to_string();
    }
    c = c.replace("\r", "\\r");
    c = c.replace("\n", "\\n");
    c = c.replace("\t", "\\t");
    return format!("[{}, {}]", token.kind, c);
}
