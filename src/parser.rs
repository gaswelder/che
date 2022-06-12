use crate::lexer::{new, Lexer, Token};
use crate::nodes::*;
use std::env;
use std::path::Path;

fn is_op(token_type: &str) -> bool {
    let ops = [
        "+", "-", "*", "/", "=", "|", "&", "~", "^", "<", ">", "?", ":", "%", "+=", "-=", "*=",
        "/=", "%=", "&=", "^=", "|=", "++", "--", "->", ".", ">>", "<<", "<=", ">=", "&&", "||",
        "==", "!=", "<<=", ">>=",
    ];
    return ops.contains(&token_type);
}

pub fn operator_strength(op: &str) -> usize {
    // The lower the operator, the stronger it binds. So, for example,
    // a = 1, b = 2 will be parsed as (a = 1), (b = 2).
    let map = [
        vec![","],
        vec![
            "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=",
        ],
        vec!["||"],
        vec!["&&"],
        vec!["|"],
        vec!["^"],
        vec!["&"],
        vec!["!=", "=="],
        vec![">", "<", ">=", "<="],
        vec!["<<", ">>"],
        vec!["+", "-"],
        vec!["*", "/", "%"],
        vec!["prefix"],
        vec!["->", "."],
    ];
    for (i, ops) in map.iter().enumerate() {
        if ops.contains(&op) {
            return i + 1;
        }
    }
    panic!("unknown operator: '{}'", op);
}

fn is_postfix_op(token: &str) -> bool {
    let ops = ["++", "--", "(", "["];
    return ops.to_vec().contains(&token);
}

fn is_prefix_op(op: &str) -> bool {
    let ops = ["!", "--", "++", "*", "~", "&", "-"];
    return ops.to_vec().contains(&op);
}

fn is_type(name: &str, typenames: &Vec<String>) -> bool {
    let types = [
        "struct",
        "enum",
        "union",
        "void",
        "char",
        "short",
        "int",
        "long",
        "float",
        "double",
        "unsigned",
        "bool",
        "va_list",
        "FILE",
        "ptrdiff_t",
        "size_t",
        "wchar_t",
        "int8_t",
        "int16_t",
        "int32_t",
        "int64_t",
        "uint8_t",
        "uint16_t",
        "uint32_t",
        "uint64_t",
        "clock_t",
        "time_t",
        "fd_set",
        "socklen_t",
        "ssize_t",
    ];
    return types.to_vec().contains(&name)
        || typenames.to_vec().contains(&String::from(name))
        || name.ends_with("_t");
}

fn with_comment(comment: Option<&str>, message: String) -> String {
    match comment {
        Some(s) => format!("{}: {}", s, message),
        None => message.to_string(),
    }
}

fn expect(lexer: &mut Lexer, kind: &str, comment: Option<&str>) -> Result<Token, String> {
    if !lexer.more() {
        return Err(with_comment(
            comment,
            format!("expected '{}', got end of file", kind),
        ));
    }
    let next = lexer.peek().unwrap();
    if next.kind != kind {
        return Err(with_comment(
            comment,
            format!("expected '{}', got '{}' at {}", kind, next.kind, next.pos),
        ));
    }
    return Ok(lexer.get().unwrap());
}

fn parse_identifier(lexer: &mut Lexer) -> Result<String, String> {
    let identifier = String::from(expect(lexer, "word", None)?.content.unwrap());
    return Ok(identifier);
}

fn parse_type(lexer: &mut Lexer, comment: Option<&str>) -> Result<Type, String> {
    let mut is_const = false;
    let type_name: String;

    if lexer.follows("const") {
        lexer.get();
        is_const = true;
    }

    if lexer.follows("struct") {
        lexer.get();
        let name = expect(lexer, "word", comment)?.content.unwrap();
        type_name = format!("struct {}", name);
    } else {
        type_name = expect(lexer, "word", comment)?.content.unwrap();
    }

    return Ok(Type {
        is_const,
        type_name,
    });
}

fn parse_anonymous_typeform(lexer: &mut Lexer) -> Result<AnonymousTypeform, String> {
    let type_name = parse_type(lexer, None).unwrap();
    let mut ops: Vec<String> = Vec::new();
    while lexer.follows("*") {
        ops.push(lexer.get().unwrap().kind);
    }
    return Ok(AnonymousTypeform { type_name, ops });
}

fn parse_anonymous_parameters(lexer: &mut Lexer) -> Result<AnonymousParameters, String> {
    let mut forms: Vec<AnonymousTypeform> = Vec::new();
    expect(lexer, "(", Some("anonymous function parameters")).unwrap();
    if !lexer.follows(")") {
        forms.push(parse_anonymous_typeform(lexer).unwrap());
        while lexer.follows(",") {
            lexer.get();
            forms.push(parse_anonymous_typeform(lexer).unwrap());
        }
    }
    expect(lexer, ")", Some("anonymous function parameters")).unwrap();
    return Ok(AnonymousParameters { forms });
}

fn parse_literal(lexer: &mut Lexer) -> Result<Literal, String> {
    let types = ["string", "num", "char"];
    for t in types.iter() {
        if lexer.peek().unwrap().kind != t.to_string() {
            continue;
        }
        let value = lexer.get().unwrap().content.unwrap();
        return Ok(Literal {
            type_name: t.to_string(),
            value: value,
        });
    }
    let next = lexer.peek().unwrap();
    if next.kind == "word" && next.content.as_ref().unwrap() == "NULL" {
        lexer.get();
        return Ok(Literal {
            type_name: "null".to_string(),
            value: "NULL".to_string(),
        });
    }
    return Err(format!("literal expected, got {}", lexer.peek().unwrap()));
}

fn parse_enum(
    lexer: &mut Lexer,
    is_pub: bool,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<Enum, String> {
    let mut members: Vec<EnumMember> = Vec::new();
    expect(lexer, "enum", Some("enum definition"))?;
    expect(lexer, "{", Some("enum definition"))?;
    loop {
        let id = parse_identifier(lexer)?;
        let value: Option<Expression> = if lexer.eat("=") {
            Some(parse_expression(lexer, 0, typenames, modnames)?)
        } else {
            None
        };
        members.push(EnumMember { id, value });
        if lexer.eat(",") {
            continue;
        } else {
            break;
        }
    }
    expect(lexer, "}", Some("enum definition"))?;
    expect(lexer, ";", Some("enum definition"))?;
    return Ok(Enum { is_pub, members });
}

fn parse_array_literal(lexer: &mut Lexer) -> Result<ArrayLiteral, String> {
    let mut values: Vec<ArrayLiteralEntry> = Vec::new();
    expect(lexer, "{", Some("array literal"))?;
    if !lexer.follows("}") {
        values.push(parse_array_literal_entry(lexer)?);
        while lexer.eat(",") {
            if lexer.peek_skipping_comments().unwrap().kind == "}" {
                break;
            }
            values.push(parse_array_literal_entry(lexer)?);
        }
    }
    expect(lexer, "}", Some("array literal"))?;
    return Ok(ArrayLiteral { values });
}

fn parse_array_literal_entry(lexer: &mut Lexer) -> Result<ArrayLiteralEntry, String> {
    let mut index: ArrayLiteralKey = ArrayLiteralKey::None;
    if lexer.follows("[") {
        lexer.get();
        if lexer.follows("word") {
            index = ArrayLiteralKey::Identifier(parse_identifier(lexer)?);
        } else {
            index = ArrayLiteralKey::Literal(parse_literal(lexer)?);
        }
        expect(lexer, "]", Some("array literal entry"))?;
        expect(lexer, "=", Some("array literal entry"))?;
    }
    let value: ArrayLiteralValue;
    if lexer.follows("{") {
        value = ArrayLiteralValue::ArrayLiteral(parse_array_literal(lexer)?);
    } else if lexer.follows("word") {
        value = ArrayLiteralValue::Identifier(parse_identifier(lexer)?);
    } else {
        value = ArrayLiteralValue::Literal(parse_literal(lexer)?);
    }

    return Ok(ArrayLiteralEntry { index, value });
}

fn parse_expression(
    lexer: &mut Lexer,
    current_strength: usize,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<Expression, String> {
    let mut result: Expression = parse_atom(lexer, typenames, modnames)?;
    loop {
        let peek = lexer.peek().unwrap();
        if !is_op(&peek.kind) {
            break;
        }
        // If the operator is not stronger that our current level,
        // yield the result.
        if operator_strength(peek.kind.as_str()) <= current_strength {
            return Ok(Expression::Expression(Box::new(result)));
        }
        let op = lexer.get().unwrap();
        let next = parse_expression(lexer, operator_strength(&op.kind), typenames, modnames)?;
        result = Expression::BinaryOp {
            op: String::from(&op.kind),
            a: Box::new(result),
            b: Box::new(next),
        };
    }
    return Ok(Expression::Expression(Box::new(result)));
}

fn parse_atom(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<Expression, String> {
    let nono = ["case", "default", "if", "else", "for", "while", "switch"];
    let next = &lexer.peek().unwrap().kind;
    if nono.contains(&next.as_str()) {
        return Err(format!(
            "expression: unexpected keyword '{}'",
            lexer.peek().unwrap().kind
        ));
    }

    // Typecast?
    if next == "("
        && lexer.peek_n(1).unwrap().kind == "word"
        && is_type(
            lexer.peek_n(1).unwrap().content.as_ref().unwrap(),
            typenames,
        )
    {
        expect(lexer, "(", None)?;
        let typeform = parse_anonymous_typeform(lexer)?;
        expect(lexer, ")", Some("typecast"))?;
        return Ok(Expression::Cast(Box::new(Cast {
            type_name: typeform,
            operand: parse_expression(lexer, 0, typenames, modnames).unwrap(),
        })));
    }

    // Parenthesized expression?
    if next == "(" {
        lexer.get();
        let expr = parse_expression(lexer, 0, typenames, modnames)?;
        expect(lexer, ")", None)?;
        return Ok(Expression::Expression(Box::new(expr)));
    }

    // Composite literal?
    if next == "{" {
        if lexer.peek_n(1).unwrap().kind == "." {
            return Ok(Expression::StructLiteral(Box::new(
                parse_struct_literal(lexer, typenames, modnames).unwrap(),
            )));
        }
        return Ok(Expression::ArrayLiteral(Box::new(parse_array_literal(
            lexer,
        )?)));
    }

    // Sizeof?
    if next == "sizeof" {
        return Ok(Expression::Sizeof(Box::new(parse_sizeof(
            lexer, typenames, modnames,
        )?)));
    }

    if is_prefix_op(next) {
        let op = lexer.get().unwrap().kind;
        let operand = parse_expression(lexer, operator_strength("prefix"), typenames, modnames)?;
        return Ok(Expression::PrefixOperator(Box::new(PrefixOperator {
            operator: op,
            operand: operand,
        })));
    }

    let mut result: Expression;

    // mod "." word ?
    if next == "word"
        && lexer.peek_n(1).unwrap().kind == "."
        && lexer.peek_n(2).unwrap().kind == "word"
        && modnames.contains(lexer.peek_n(0).unwrap().content.as_ref().unwrap())
    {
        let t1 = lexer.get().unwrap();
        lexer.get();
        let identifier = String::from(expect(lexer, "word", None)?.content.unwrap());
        result = Expression::Identifier(format!("{}_{}", t1.content.unwrap(), identifier));
    }
    // word ?
    else if next == "word" {
        result = Expression::Identifier(parse_identifier(lexer)?);
    }
    // must be some literal
    else {
        result = Expression::Literal(parse_literal(lexer)?);
    }

    while lexer.more() {
        if lexer.peek().unwrap().kind == "(" {
            result = Expression::FunctionCall(Box::new(parse_function_call(
                lexer, typenames, modnames, result,
            )?));
            continue;
        }
        if lexer.peek().unwrap().kind == "[" {
            expect(lexer, "[", Some("array index"))?;
            let index = parse_expression(lexer, 0, typenames, modnames)?;
            expect(lexer, "]", Some("array index"))?;
            result = Expression::ArrayIndex(Box::new(ArrayIndex {
                array: result,
                index: index,
            }));
            continue;
        }

        if is_postfix_op(lexer.peek().unwrap().kind.as_str()) {
            let op = lexer.get().unwrap().kind;
            result = Expression::PostfixOperator(Box::new(PostfixOperator {
                operand: result,
                operator: op,
            }));
            continue;
        }
        break;
    }

    return Ok(result);
}

fn parse_struct_literal(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<StructLiteral, String> {
    let mut result = StructLiteral {
        members: Vec::new(),
    };
    expect(lexer, "{", Some("struct literal"))?;
    loop {
        expect(lexer, ".", Some("struct literal member"))?;
        let member_name = parse_identifier(lexer)?;
        expect(lexer, "=", Some("struct literal member"))?;
        let member_value = parse_expression(lexer, 0, typenames, modnames)?;
        result.members.push(StructLiteralMember {
            name: member_name,
            value: member_value,
        });
        if lexer.follows(",") {
            lexer.get();
            continue;
        } else {
            break;
        }
    }
    expect(lexer, "}", Some("struct literal"))?;
    return Ok(result);
}

fn parse_sizeof(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<Sizeof, String> {
    expect(lexer, "sizeof", None)?;
    expect(lexer, "(", None)?;
    let argument: SizeofArgument;

    if lexer.peek().unwrap().kind == "word"
        && is_type(lexer.peek().unwrap().content.as_ref().unwrap(), typenames)
    {
        argument = SizeofArgument::Type(parse_type(lexer, None)?);
    } else {
        argument =
            SizeofArgument::Expression(parse_expression(lexer, 0, typenames, modnames).unwrap());
    }
    expect(lexer, ")", None)?;
    return Ok(Sizeof { argument });
}

fn parse_function_call(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
    function_name: Expression,
) -> Result<FunctionCall, String> {
    let mut arguments: Vec<Expression> = Vec::new();
    expect(lexer, "(", None)?;
    if lexer.more() && lexer.peek().unwrap().kind != ")" {
        arguments.push(parse_expression(lexer, 0, typenames, modnames)?);
        while lexer.follows(",") {
            lexer.get();
            arguments.push(parse_expression(lexer, 0, typenames, modnames)?);
        }
    }
    expect(lexer, ")", None)?;
    return Ok(FunctionCall {
        function: function_name,
        arguments: arguments,
    });
}

fn parse_while(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<While, String> {
    expect(lexer, "while", None)?;
    expect(lexer, "(", None)?;
    let condition = parse_expression(lexer, 0, typenames, modnames)?;
    expect(lexer, ")", None)?;
    let body = parse_body(lexer, typenames, modnames)?;
    return Ok(While { condition, body });
}

fn parse_body(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<Body, String> {
    let mut statements: Vec<Statement> = Vec::new();
    if lexer.follows("{") {
        expect(lexer, "{", None)?;
        while !lexer.follows("}") {
            statements.push(parse_statement(lexer, typenames, modnames)?);
        }
        expect(lexer, "}", None)?;
    } else {
        statements.push(parse_statement(lexer, typenames, modnames)?);
    }
    return Ok(Body { statements });
}

fn parse_statement(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<Statement, String> {
    let next = lexer.peek().unwrap();
    if (next.kind == "word" && is_type(&next.content.as_ref().unwrap(), typenames))
        || next.kind == "const"
    {
        return parse_variable_declaration(lexer, typenames, modnames);
    }

    match next.kind.as_str() {
        "if" => return Ok(Statement::If(parse_if(lexer, typenames, modnames)?)),
        "for" => return Ok(Statement::For(parse_for(lexer, typenames, modnames)?)),
        "while" => return Ok(Statement::While(parse_while(lexer, typenames, modnames)?)),
        "return" => return Ok(Statement::Return(parse_return(lexer, typenames, modnames)?)),
        "switch" => return Ok(Statement::Switch(parse_switch(lexer, typenames, modnames)?)),
        _ => {
            let expr = parse_expression(lexer, 0, typenames, modnames)?;
            expect(lexer, ";", Some("parsing statement"))?;
            return Ok(Statement::Expression(expr));
        }
    };
}

fn parse_variable_declaration(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<Statement, String> {
    let type_name = parse_type(lexer, None)?;
    let mut forms = vec![];
    let mut values = vec![];
    loop {
        forms.push(parse_form(lexer, typenames, modnames)?);
        if lexer.eat("=") {
            values.push(Some(parse_expression(lexer, 0, typenames, modnames)?));
        } else {
            values.push(None)
        };
        if !lexer.eat(",") {
            break;
        }
    }
    expect(lexer, ";", None)?;
    return Ok(Statement::VariableDeclaration {
        type_name,
        forms,
        values,
    });
}

fn parse_return(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<Return, String> {
    expect(lexer, "return", None)?;
    if lexer.peek().unwrap().kind == ";" {
        lexer.get();
        return Ok(Return { expression: None });
    }
    let expression = parse_expression(lexer, 0, typenames, modnames)?;
    expect(lexer, ";", None)?;
    return Ok(Return {
        expression: Some(expression),
    });
}

fn parse_form(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<Form, String> {
    // *argv[]
    // linechars[]
    // buf[SIZE * 2]
    let mut node = Form {
        stars: String::new(),
        name: String::new(),
        indexes: vec![],
    };

    while lexer.follows("*") {
        node.stars += &lexer.get().unwrap().kind;
    }

    match expect(lexer, "word", None)?.content {
        Some(x) => {
            node.name = String::from(x);
        }
        None => {
            return Err(String::from("missing word content"));
        }
    }

    while lexer.follows("[") {
        &lexer.get().unwrap();
        let expr: Option<Expression>;
        if lexer.more() && lexer.peek().unwrap().kind != "]" {
            expr = Some(parse_expression(lexer, 0, typenames, modnames)?);
        } else {
            expr = None;
        }
        node.indexes.push(expr);
        expect(lexer, "]", None)?;
    }
    return Ok(node);
}

fn parse_if(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<If, String> {
    let condition;
    let body;
    let mut else_body = None;

    expect(lexer, "if", Some("if statement"))?;
    expect(lexer, "(", Some("if statement"))?;
    condition = parse_expression(lexer, 0, typenames, modnames)?;
    expect(lexer, ")", Some("if statement"))?;
    body = parse_body(lexer, typenames, modnames)?;
    if lexer.follows("else") {
        lexer.get();
        else_body = Some(parse_body(lexer, typenames, modnames)?);
    }
    return Ok(If {
        condition,
        body,
        else_body,
    });
}

fn parse_for(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<For, String> {
    expect(lexer, "for", None)?;
    expect(lexer, "(", None)?;

    let init: ForInit;
    if lexer.peek().unwrap().kind == "word"
        && is_type(&lexer.peek().unwrap().content.as_ref().unwrap(), typenames)
    {
        let type_name = parse_type(lexer, None)?;
        let form = parse_form(lexer, typenames, modnames)?;
        expect(lexer, "=", None)?;
        let value = parse_expression(lexer, 0, typenames, modnames)?;
        init = ForInit::LoopCounterDeclaration(LoopCounterDeclaration {
            type_name,
            form,
            value,
        });
    } else {
        init = ForInit::Expression(parse_expression(lexer, 0, typenames, modnames)?);
    }

    expect(lexer, ";", None)?;
    let condition = parse_expression(lexer, 0, typenames, modnames)?;
    expect(lexer, ";", None)?;
    let action = parse_expression(lexer, 0, typenames, modnames)?;
    expect(lexer, ")", None)?;
    let body = parse_body(lexer, typenames, modnames)?;

    return Ok(For {
        init,
        condition,
        action,
        body,
    });
}

fn parse_switch(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<Switch, String> {
    expect(lexer, "switch", None)?;
    expect(lexer, "(", None)?;
    let value = parse_expression(lexer, 0, typenames, modnames)?;
    let mut cases: Vec<SwitchCase> = vec![];
    expect(lexer, ")", None)?;
    expect(lexer, "{", None)?;
    while lexer.follows("case") {
        expect(lexer, "case", None)?;
        let case_value = if lexer.follows("word") {
            SwitchCaseValue::Identifier(parse_identifier(lexer)?)
        } else {
            SwitchCaseValue::Literal(parse_literal(lexer)?)
        };
        expect(lexer, ":", None)?;
        let until = ["case", "break", "default", "}"];
        let mut statements: Vec<Statement> = vec![];
        while lexer.more() && !until.contains(&lexer.peek().unwrap().kind.as_str()) {
            statements.push(parse_statement(lexer, typenames, modnames)?);
        }
        cases.push(SwitchCase {
            value: case_value,
            statements,
        });
    }
    let mut default: Option<Vec<Statement>> = None;
    if lexer.follows("default") {
        expect(lexer, "default", None)?;
        expect(lexer, ":", None)?;
        let mut def: Vec<Statement> = vec![];
        while lexer.more() && lexer.peek().unwrap().kind != "}" {
            def.push(parse_statement(lexer, typenames, modnames)?);
        }
        default = Some(def);
    }
    expect(lexer, "}", None)?;
    return Ok(Switch {
        value,
        cases,
        default,
    });
}

fn parse_function_declaration(
    lexer: &mut Lexer,
    is_pub: bool,
    type_name: Type,
    form: Form,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<FunctionDeclaration, String> {
    let mut parameters: Vec<FunctionParameter> = vec![];
    let mut variadic = false;
    expect(lexer, "(", None)?;
    if !lexer.follows(")") {
        parameters.push(parse_function_parameter(lexer, typenames, modnames)?);
        while lexer.follows(",") {
            lexer.get();
            if lexer.follows("...") {
                lexer.get();
                variadic = true;
                break;
            }
            parameters.push(parse_function_parameter(lexer, typenames, modnames)?);
        }
    }
    expect(lexer, ")", None)?;
    let body = parse_body(lexer, typenames, modnames)?;
    return Ok(FunctionDeclaration {
        is_pub,
        type_name,
        form,
        parameters: FunctionParameters {
            list: parameters,
            variadic,
        },
        body,
    });
}

fn parse_function_parameter(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<FunctionParameter, String> {
    let mut forms: Vec<Form> = vec![];
    let type_name = parse_type(lexer, None)?;
    forms.push(parse_form(lexer, typenames, modnames)?);
    while lexer.follows(",")
        && lexer.peek_n(1).unwrap().kind != "..."
        && lexer.peek_n(1).unwrap().kind != "const"
        && !(lexer.peek_n(1).unwrap().kind == "word"
            && is_type(
                lexer.peek_n(1).unwrap().content.as_ref().unwrap(),
                typenames,
            ))
    {
        lexer.get();
        forms.push(parse_form(lexer, typenames, modnames)?);
    }
    return Ok(FunctionParameter { type_name, forms });
}

fn parse_union(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<Union, String> {
    let mut fields: Vec<UnionField> = vec![];
    expect(lexer, "union", None)?;
    expect(lexer, "{", None)?;
    while !lexer.follows("}") {
        let type_name = parse_type(lexer, None)?;
        let form = parse_form(lexer, typenames, modnames)?;
        expect(lexer, ";", None)?;
        fields.push(UnionField { type_name, form });
    }
    expect(lexer, "}", None)?;
    let form = parse_form(lexer, typenames, modnames)?;
    expect(lexer, ";", None)?;
    return Ok(Union { form, fields });
}

fn parse_module_object(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<ModuleObject, String> {
    let mut is_pub = false;
    if lexer.follows("pub") {
        lexer.get();
        is_pub = true;
    }
    if lexer.follows("enum") {
        return Ok(ModuleObject::Enum(parse_enum(
            lexer, is_pub, typenames, modnames,
        )?));
    }
    if lexer.follows("typedef") {
        return Ok(ModuleObject::Typedef(parse_typedef(
            is_pub, lexer, typenames, modnames,
        )?));
    }
    let type_name = parse_type(lexer, None)?;
    let form = parse_form(lexer, typenames, modnames)?;
    if lexer.peek().unwrap().kind == "(" {
        return Ok(ModuleObject::FunctionDeclaration(
            parse_function_declaration(lexer, is_pub, type_name, form, typenames, modnames)?,
        ));
    }

    if lexer.peek().unwrap().kind != "=" {
        return Err("module variable: '=' expected".to_string());
    }

    if lexer.peek().unwrap().kind == "=" {
        if is_pub {
            return Err("module variables can't be exported".to_string());
        }
        lexer.get();
        let value = parse_expression(lexer, 0, typenames, modnames)?;
        expect(lexer, ";", Some("module variable declaration"))?;
        let var = ModuleVariable {
            type_name,
            form,
            value,
        };
        return Ok(ModuleObject::ModuleVariable(var));
    }
    return Err("unexpected input".to_string());
}

fn parse_compat_macro(lexer: &mut Lexer) -> Result<CompatMacro, String> {
    let content = expect(lexer, "macro", None)
        .unwrap()
        .content
        .unwrap()
        .clone();
    let pos = content.find(" ");
    if pos.is_none() {
        return Err(format!("can't get macro name from '{}'", content));
    }
    let (name, value) = content.split_at(pos.unwrap());

    return Ok(CompatMacro {
        name: name[1..].to_string(),
        value: value.to_string(),
    });
}

fn parse_typedef(
    is_pub: bool,
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<Typedef, String> {
    // typedef void *f(int a)[20];
    // typedef foo bar;
    // typedef struct foo foo_t;
    expect(lexer, "typedef", None)?;
    let type_name;
    if lexer.follows("{") {
        type_name =
            TypedefTarget::AnonymousStruct(parse_anonymous_struct(lexer, typenames, modnames)?);
    } else {
        type_name = TypedefTarget::Type(parse_type(lexer, Some("typedef"))?);
    }
    let form = parse_typedef_form(lexer)?;
    expect(lexer, ";", Some("typedef"))?;
    return Ok(Typedef {
        is_pub,
        type_name,
        form,
    });
}

fn parse_typedef_form(lexer: &mut Lexer) -> Result<TypedefForm, String> {
    let mut stars = String::new();
    while lexer.follows("*") {
        stars += &lexer.get().unwrap().kind;
    }

    let alias = parse_identifier(lexer)?;

    let mut params: Option<AnonymousParameters> = None;
    if lexer.follows("(") {
        params = Some(parse_anonymous_parameters(lexer)?);
    }

    let mut size: usize = 0;
    if lexer.follows("[") {
        lexer.get();
        size = expect(lexer, "num", None)
            .unwrap()
            .content
            .unwrap()
            .parse()
            .unwrap();
        expect(lexer, "]", None)?;
    }

    return Ok(TypedefForm {
        stars,
        params,
        size,
        alias,
    });
}

fn parse_anonymous_struct(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<AnonymousStruct, String> {
    let mut fieldlists: Vec<StructEntry> = vec![];
    expect(lexer, "{", Some("struct type definition"))?;
    while lexer.more() && lexer.peek().unwrap().kind != "}" {
        if lexer.peek().unwrap().kind == "union" {
            fieldlists.push(StructEntry::Union(parse_union(lexer, typenames, modnames)?));
        } else {
            fieldlists.push(StructEntry::StructFieldlist(parse_struct_fieldlist(
                lexer, typenames, modnames,
            )?));
        }
    }
    expect(lexer, "}", None)?;

    return Ok(AnonymousStruct {
        entries: fieldlists,
    });
}

fn parse_struct_fieldlist(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    modnames: &Vec<String>,
) -> Result<StructFieldlist, String> {
    if lexer.follows("struct") {
        return Err("can't parse nested structs, please consider a typedef".to_string());
    }

    let type_name = parse_type(lexer, None)?;

    let mut forms: Vec<Form> = vec![];
    forms.push(parse_form(lexer, typenames, modnames)?);
    while lexer.follows(",") {
        lexer.get();
        forms.push(parse_form(lexer, typenames, modnames)?);
    }

    expect(lexer, ";", None)?;
    return Ok(StructFieldlist { type_name, forms });
}

fn skip_brackets(lexer: &mut Lexer) {
    expect(lexer, "{", None).unwrap();
    while lexer.more() {
        if lexer.follows("{") {
            skip_brackets(lexer);
            continue;
        }
        if lexer.follows("}") {
            break;
        }
        lexer.get();
    }
    expect(lexer, "}", None).unwrap();
}

fn get_typename(lexer: &mut Lexer) -> Result<String, String> {
    // The type name is at the end of the typedef statement.
    // typedef foo bar;
    // typedef {...} bar;
    // typedef struct foo bar;

    if lexer.follows("{") {
        skip_brackets(lexer);
        let name = expect(lexer, "word", None).unwrap().content.unwrap();
        expect(lexer, ";", None)?;
        return Ok(name);
    }

    // Get all tokens until the semicolon.
    let mut buf: Vec<Token> = vec![];
    while !lexer.ended() {
        let t = lexer.get().unwrap();
        if t.kind == ";" {
            break;
        }
        buf.push(t);
    }

    if buf.is_empty() {
        return Err("No tokens after 'typedef'".to_string());
    }

    buf.reverse();

    // We assume that function typedefs end with "(...)".
    // In that case we omit that part.
    if buf[0].kind == ")" {
        while !buf.is_empty() {
            let t = buf.remove(0);
            if t.kind == "(" {
                break;
            }
        }
    }

    let mut name: Option<String> = None;

    // The last "word" token is assumed to be the type name.
    while !buf.is_empty() {
        let t = buf.remove(0);
        if t.kind == "word" {
            name = Some(t.content.unwrap());
            break;
        }
    }

    if name.is_none() {
        return Err("Type name expected in the typedef".to_string());
    }
    return Ok(name.unwrap());
}

fn path_to_ns(path: String) -> String {
    return if path.contains("/") {
        path.split("/").last().unwrap().to_string()
    } else {
        path
    };
}

fn get_file_deps(path: &str) -> Result<Vec<String>, String> {
    let mut lexer = new(path);
    let mut list: Vec<String> = vec![];
    loop {
        match lexer.get() {
            None => break,
            Some(t) => {
                if t.kind == "import" {
                    list.push(path_to_ns(t.content.unwrap()))
                }
            }
        }
    }
    return Ok(list);
}

fn get_file_typenames(path: &str) -> Result<Vec<String>, String> {
    // Scan file tokens for 'typedef' keywords
    let mut lexer = new(path);
    let mut list: Vec<String> = vec![];
    loop {
        let t = lexer.get();
        if t.is_none() {
            break;
        }

        let tt = t.unwrap();
        // When a 'typedef' is encountered, look ahead
        // to find the type name
        if tt.kind == "typedef" {
            list.push(get_typename(&mut lexer)?);
        }
        if tt.kind == "macro" && tt.content.as_ref().unwrap().starts_with("#type") {
            let name = tt.content.unwrap()[6..].trim().to_string();
            list.push(name);
        }
    }
    return Ok(list);
}

pub fn get_module(name: &String) -> Result<Module, String> {
    // If requested module name ends with ".c", we look for it at the given
    // location. If ".c" is omitted, we look for it inside the hardcoded lib
    // directory.
    let module_path = if name.ends_with(".c") {
        name.clone()
    } else {
        let path = env::var("CHELANG_HOME").unwrap_or(String::from("."));
        format!("{}/lib/{}.c", path, name)
    };

    if std::fs::metadata(&module_path).is_err() {
        return Err(format!(
            "can't find module '{}' (looked at {})",
            name, module_path
        ));
    }

    let types = get_file_typenames(&module_path)?;
    let deps = get_file_deps(&module_path)?;
    let mut lexer = new(&module_path);
    let elements = parse_module(&mut lexer, types, deps);
    if elements.is_err() {
        let next = lexer.peek().unwrap();
        let wher = format!("{}: {}", module_path, lexer.peek().unwrap().pos);
        let what = elements.err().unwrap();
        return Err(format!("{}: {}: {}...", wher, what, token_to_string(next)));
    }
    return Ok(Module {
        elements: elements.unwrap(),
        id: format!("{}", Path::new(&module_path).display()),
    });
}

fn token_to_string(token: &Token) -> String {
    if token.content.is_none() {
        return format!("[{}]", token.kind);
    }

    let n = 40;
    let mut c: String;
    if token.content.as_ref().unwrap().len() > n {
        c = token.content.as_ref().unwrap()[0..(n - 3)].to_string() + "...";
    } else {
        c = token.content.as_ref().unwrap().to_string();
    }
    c = c.replace("\r", "\\r");
    c = c.replace("\n", "\\n");
    c = c.replace("\t", "\\t");
    return format!("[{}, {}]", token.kind, c);
}

fn parse_module(
    lexer: &mut Lexer,
    typenames: Vec<String>,
    modnames: Vec<String>,
) -> Result<Vec<ModuleObject>, String> {
    let mut elements: Vec<ModuleObject> = vec![];
    let mut types = typenames.clone();
    while lexer.more() {
        match lexer.peek().unwrap().kind.as_str() {
            "import" => {
                let tok = lexer.get().unwrap();
                let path = tok.content.unwrap();
                let p = path.clone();
                elements.push(ModuleObject::Import(Import { path }));
                let module = get_module(&p)?;
                for element in module.elements {
                    match element {
                        ModuleObject::Typedef(t) => {
                            types.push(t.form.alias);
                        }
                        _ => {}
                    }
                }
            }
            "macro" => {
                elements.push(ModuleObject::CompatMacro(parse_compat_macro(lexer)?));
            }
            _ => {
                elements.push(parse_module_object(lexer, &types, &modnames)?);
            }
        }
    }
    return Ok(elements);
}
