use crate::lexer::{new, Lexer, Token};
use serde::{Deserialize, Serialize, Serializer};

pub fn is_op(token_type: &str) -> bool {
    let ops = [
        "+", "-", "*", "/", "=", "|", "&", "~", "^", "<", ">", "?", ":", "%", "+=", "-=", "*=",
        "/=", "%=", "&=", "^=", "|=", "++", "--", "->", ".", ">>", "<<", "<=", ">=", "&&", "||",
        "==", "!=", "<<=", ">>=",
    ];
    return ops.contains(&token_type);
}

pub fn operator_strength(op: &str) -> usize {
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

pub fn is_postfix_op(token: &str) -> bool {
    let ops = ["++", "--", "(", "["];
    return ops.to_vec().contains(&token);
}

pub fn is_prefix_op(op: &str) -> bool {
    let ops = ["!", "--", "++", "*", "~", "&", "-"];
    return ops.to_vec().contains(&op);
}

pub fn is_type(name: &str, typenames: &Vec<String>) -> bool {
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

pub fn expect(lexer: &mut Lexer, kind: &str, comment: Option<&str>) -> Result<Token, String> {
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
            format!("expected '{}', got {} at {}", kind, next.kind, next.pos),
        ));
    }
    return Ok(lexer.get().unwrap());
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Identifier {
    kind: String,
    name: String,
}

pub fn parse_identifier(lexer: &mut Lexer) -> Result<Identifier, String> {
    let tok = expect(lexer, "word", None)?;
    let name = tok.content;
    return Ok(Identifier {
        kind: "c_identifier".to_string(),
        name: name.unwrap(),
    });
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Type {
    kind: String,
    is_const: bool,
    type_name: String,
}

pub fn parse_type(lexer: &mut Lexer, comment: Option<&str>) -> Result<Type, String> {
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
        kind: "c_type".to_string(),
        is_const,
        type_name,
    });
}

#[derive(Serialize, Deserialize, Debug)]
pub struct AnonymousTypeform {
    kind: String,
    type_name: Type,
    ops: Vec<String>,
}

pub fn parse_anonymous_typeform(lexer: &mut Lexer) -> Result<AnonymousTypeform, String> {
    let type_name = parse_type(lexer, None).unwrap();
    let mut ops: Vec<String> = Vec::new();
    while lexer.follows("*") {
        ops.push(lexer.get().unwrap().kind);
    }
    return Ok(AnonymousTypeform {
        kind: "c_anonymous_typeform".to_string(),
        type_name: type_name,
        ops,
    });
}

#[derive(Serialize, Deserialize, Debug)]
pub struct AnonymousParameters {
    kind: String,
    forms: Vec<AnonymousTypeform>,
}
pub fn parse_anonymous_parameters(lexer: &mut Lexer) -> Result<AnonymousParameters, String> {
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
    return Ok(AnonymousParameters {
        kind: "c_anonymous_parameters".to_string(),
        forms,
    });
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Literal {
    kind: String,
    type_name: String,
    value: String,
}
pub fn parse_literal(lexer: &mut Lexer) -> Result<Literal, String> {
    let types = ["string", "num", "char"];
    for t in types.iter() {
        if lexer.peek().unwrap().kind != t.to_string() {
            continue;
        }
        let value = lexer.get().unwrap().content.unwrap();
        return Ok(Literal {
            kind: "c_literal".to_string(),
            type_name: t.to_string(),
            value: value,
        });
    }
    let next = lexer.peek().unwrap();
    if next.kind == "word" && next.content.as_ref().unwrap() == "NULL" {
        lexer.get();
        return Ok(Literal {
            kind: "c_literal".to_string(),
            type_name: "null".to_string(),
            value: "NULL".to_string(),
        });
    }
    return Err(format!("literal expected, got {}", lexer.peek().unwrap()));
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Enum {
    kind: String,
    is_pub: bool,
    members: Vec<EnumMember>,
}
#[derive(Serialize, Deserialize, Debug)]
pub struct EnumMember {
    kind: String,
    id: Identifier,
    value: Option<Literal>,
}
pub fn parse_enum(lexer: &mut Lexer, is_pub: bool) -> Result<Enum, String> {
    let mut members: Vec<EnumMember> = Vec::new();
    expect(lexer, "enum", Some("enum definition")).unwrap();
    expect(lexer, "{", Some("enum definition")).unwrap();
    loop {
        let id = parse_identifier(lexer);
        let mut value: Option<Literal> = None;
        if lexer.follows("=") {
            lexer.get();
            value = Some(parse_literal(lexer)?);
        }
        members.push(EnumMember {
            kind: "c_enum_member".to_string(),
            id: id.unwrap(),
            value,
        });
        if lexer.follows(",") {
            lexer.get();
            continue;
        } else {
            break;
        }
    }
    expect(lexer, "}", Some("enum definition")).unwrap();
    expect(lexer, ";", Some("enum definition")).unwrap();
    return Ok(Enum {
        kind: "c_enum".to_string(),
        is_pub,
        members,
    });
}

#[derive(Debug, Serialize)]
pub struct ArrayLiteral {
    kind: String,
    values: Vec<ArrayLiteralEntry>,
}

pub fn parse_array_literal(lexer: &mut Lexer) -> Result<ArrayLiteral, String> {
    let mut values: Vec<ArrayLiteralEntry> = Vec::new();
    expect(lexer, "{", Some("array literal"))?;
    if !lexer.follows("}") {
        values.push(parse_array_literal_entry(lexer).unwrap());
        while lexer.follows(",") {
            lexer.get();
            values.push(parse_array_literal_entry(lexer).unwrap());
        }
    }
    expect(lexer, "}", Some("array literal"))?;
    return Ok(ArrayLiteral {
        kind: "c_array_literal".to_string(),
        values,
    });
}

#[derive(Debug)]
enum ArrayLiteralKey {
    None,
    Identifier(Identifier),
    Literal(Literal),
}

impl Serialize for ArrayLiteralKey {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            ArrayLiteralKey::None => serializer.serialize_none(),
            ArrayLiteralKey::Identifier(x) => Serialize::serialize(x, serializer),
            ArrayLiteralKey::Literal(x) => Serialize::serialize(x, serializer),
        }
    }
}

#[derive(Debug)]
enum ArrayLiteralValue {
    ArrayLiteral(ArrayLiteral),
    Identifier(Identifier),
    Literal(Literal),
}

impl Serialize for ArrayLiteralValue {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            ArrayLiteralValue::ArrayLiteral(x) => Serialize::serialize(x, serializer),
            ArrayLiteralValue::Identifier(x) => Serialize::serialize(x, serializer),
            ArrayLiteralValue::Literal(x) => Serialize::serialize(x, serializer),
        }
    }
}

#[derive(Debug, Serialize)]
pub struct ArrayLiteralEntry {
    index: ArrayLiteralKey,
    value: ArrayLiteralValue,
}

pub fn parse_array_literal_entry(lexer: &mut Lexer) -> Result<ArrayLiteralEntry, String> {
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

#[derive(Debug, Serialize)]
pub struct BinaryOp {
    kind: String,
    op: String,
    a: Expression,
    b: Expression,
}

#[derive(Debug)]
pub enum Expression {
    BinaryOp(Box<BinaryOp>),
    Cast(Box<Cast>),
    FunctionCall(Box<FunctionCall>),
    Expression(Box<Expression>),
    Literal(Literal),
    Identifier(Identifier),
    StructLiteral(Box<StructLiteral>),
    ArrayLiteral(Box<ArrayLiteral>),
    Sizeof(Box<Sizeof>),
    PrefixOperator(Box<PrefixOperator>),
    PostfixOperator(Box<PostfixOperator>),
    ArrayIndex(Box<ArrayIndex>),
}

impl Serialize for Expression {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            Expression::BinaryOp(x) => Serialize::serialize(x, serializer),
            Expression::Cast(x) => Serialize::serialize(x, serializer),
            Expression::FunctionCall(x) => Serialize::serialize(x, serializer),
            Expression::Expression(x) => Serialize::serialize(x, serializer),
            Expression::Literal(x) => Serialize::serialize(x, serializer),
            Expression::Identifier(x) => Serialize::serialize(x, serializer),
            Expression::StructLiteral(x) => Serialize::serialize(x, serializer),
            Expression::ArrayLiteral(x) => Serialize::serialize(x, serializer),
            Expression::Sizeof(x) => Serialize::serialize(x, serializer),
            Expression::PrefixOperator(x) => Serialize::serialize(x, serializer),
            Expression::PostfixOperator(x) => Serialize::serialize(x, serializer),
            Expression::ArrayIndex(x) => Serialize::serialize(x, serializer),
        }
    }
}

pub fn parse_expression(
    lexer: &mut Lexer,
    current_strength: usize,
    typenames: &Vec<String>,
) -> Result<Expression, String> {
    let mut result: Expression = parse_atom(lexer, typenames)?;
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
        let next = parse_expression(lexer, operator_strength(&op.kind), typenames)?;
        result = Expression::BinaryOp(Box::new(BinaryOp {
            kind: "c_binary_op".to_string(),
            op: String::from(&op.kind),
            a: result,
            b: next,
        }));
    }
    return Ok(Expression::Expression(Box::new(result)));
}

#[derive(Debug, Serialize)]
pub struct Cast {
    kind: String,
    type_name: AnonymousTypeform,
    operand: Expression,
}

#[derive(Debug, Serialize)]
pub struct PostfixOperator {
    kind: String,
    operator: String,
    operand: Expression,
}

#[derive(Debug, Serialize)]
pub struct PrefixOperator {
    kind: String,
    operator: String,
    operand: Expression,
}

#[derive(Debug, Serialize)]
pub struct ArrayIndex {
    kind: String,
    array: Expression,
    index: Expression,
}

pub fn parse_atom(lexer: &mut Lexer, typenames: &Vec<String>) -> Result<Expression, String> {
    let nono = ["case", "default", "if", "else", "for", "while", "switch"];
    let next = &lexer.peek().unwrap().kind;
    if nono.contains(&next.as_str()) {
        return Err(format!(
            "expression: unexpected keyword '{}'",
            lexer.peek().unwrap().kind
        ));
    }

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
            kind: "c_cast".to_string(),
            type_name: typeform,
            operand: parse_expression(lexer, 0, typenames).unwrap(),
        })));
    }

    if next == "(" {
        lexer.get();
        let expr = parse_expression(lexer, 0, typenames).unwrap();
        expect(lexer, ")", None)?;
        return Ok(Expression::Expression(Box::new(expr)));
    }

    if next == "{" {
        if lexer.peek_n(1).unwrap().kind == "." {
            return Ok(Expression::StructLiteral(Box::new(
                parse_struct_literal(lexer, typenames).unwrap(),
            )));
        }
        return Ok(Expression::ArrayLiteral(Box::new(parse_array_literal(
            lexer,
        )?)));
    }

    if next == "sizeof" {
        return Ok(Expression::Sizeof(Box::new(parse_sizeof(
            lexer, typenames,
        )?)));
    }

    if is_prefix_op(next) {
        let op = lexer.get().unwrap().kind;
        let operand = parse_expression(lexer, operator_strength("prefix"), typenames)?;
        return Ok(Expression::PrefixOperator(Box::new(PrefixOperator {
            kind: "c_prefix_operator".to_string(),
            operator: op,
            operand: operand,
        })));
    }

    let mut result: Expression;
    if next == "word" {
        result = Expression::Identifier(parse_identifier(lexer)?);
    } else {
        result = Expression::Literal(parse_literal(lexer)?);
    }

    while lexer.more() {
        if lexer.peek().unwrap().kind == "(" {
            result =
                Expression::FunctionCall(Box::new(parse_function_call(lexer, typenames, result)?));
            continue;
        }
        if lexer.peek().unwrap().kind == "[" {
            expect(lexer, "[", Some("array index"))?;
            let index = parse_expression(lexer, 0, typenames)?;
            expect(lexer, "]", Some("array index"))?;
            result = Expression::ArrayIndex(Box::new(ArrayIndex {
                kind: "c_array_index".to_string(),
                array: result,
                index: index,
            }));
            continue;
        }

        if is_postfix_op(lexer.peek().unwrap().kind.as_str()) {
            let op = lexer.get().unwrap().kind;
            result = Expression::PostfixOperator(Box::new(PostfixOperator {
                kind: "c_postfix_operator".to_string(),
                operand: result,
                operator: op,
            }));
            continue;
        }
        break;
    }

    return Ok(result);
}

#[derive(Debug, Serialize)]
pub struct StructLiteralMember {
    name: Identifier,
    value: Expression,
}

#[derive(Debug, Serialize)]
pub struct StructLiteral {
    kind: String,
    members: Vec<StructLiteralMember>,
}

pub fn parse_struct_literal(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
) -> Result<StructLiteral, String> {
    let mut result = StructLiteral {
        kind: "c_struct_literal".to_string(),
        members: Vec::new(),
    };
    expect(lexer, "{", Some("struct literal"))?;
    loop {
        expect(lexer, ".", Some("struct literal member"))?;
        let member_name = parse_identifier(lexer)?;
        expect(lexer, "=", Some("struct literal member"))?;
        let member_value = parse_expression(lexer, 0, typenames)?;
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

#[derive(Debug)]
enum SizeofArgument {
    Type(Type),
    Expression(Expression),
}

impl Serialize for SizeofArgument {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            SizeofArgument::Type(x) => Serialize::serialize(x, serializer),
            SizeofArgument::Expression(x) => Serialize::serialize(x, serializer),
        }
    }
}

#[derive(Debug, Serialize)]
pub struct Sizeof {
    kind: String,
    argument: SizeofArgument,
}

pub fn parse_sizeof(lexer: &mut Lexer, typenames: &Vec<String>) -> Result<Sizeof, String> {
    expect(lexer, "sizeof", None)?;
    expect(lexer, "(", None)?;
    let argument: SizeofArgument;

    if lexer.peek().unwrap().kind == "word"
        && is_type(lexer.peek().unwrap().content.as_ref().unwrap(), typenames)
    {
        argument = SizeofArgument::Type(parse_type(lexer, None)?);
    } else {
        argument = SizeofArgument::Expression(parse_expression(lexer, 0, typenames).unwrap());
    }
    expect(lexer, ")", None)?;
    return Ok(Sizeof {
        kind: "c_sizeof".to_string(),
        argument,
    });
}

#[derive(Debug, Serialize)]
pub struct FunctionCall {
    kind: String,
    function: Expression,
    arguments: Vec<Expression>,
}

pub fn parse_function_call(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
    function_name: Expression,
) -> Result<FunctionCall, String> {
    let mut arguments: Vec<Expression> = Vec::new();
    expect(lexer, "(", None)?;
    if lexer.more() && lexer.peek().unwrap().kind != ")" {
        arguments.push(parse_expression(lexer, 0, typenames)?);
        while lexer.follows(",") {
            lexer.get();
            arguments.push(parse_expression(lexer, 0, typenames)?);
        }
    }
    expect(lexer, ")", None)?;
    return Ok(FunctionCall {
        kind: "c_function_call".to_string(),
        function: function_name,
        arguments: arguments,
    });
}

#[derive(Serialize, Debug)]
pub struct While {
    kind: String,
    condition: Expression,
    body: Body,
}

pub fn parse_while(lexer: &mut Lexer, typenames: &Vec<String>) -> Result<While, String> {
    expect(lexer, "while", None)?;
    expect(lexer, "(", None)?;
    let condition = parse_expression(lexer, 0, typenames)?;
    expect(lexer, ")", None)?;
    let body = parse_body(lexer, typenames)?;
    return Ok(While {
        kind: "c_while".to_string(),
        condition,
        body,
    });
}

#[derive(Serialize, Debug)]
pub struct Body {
    kind: String,
    statements: Vec<Statement>,
}

pub fn parse_body(lexer: &mut Lexer, typenames: &Vec<String>) -> Result<Body, String> {
    let mut statements: Vec<Statement> = Vec::new();
    if lexer.follows("{") {
        expect(lexer, "{", None)?;
        while !lexer.follows("}") {
            statements.push(parse_statement(lexer, typenames)?);
        }
        expect(lexer, "}", None)?;
    } else {
        statements.push(parse_statement(lexer, typenames)?);
    }
    return Ok(Body {
        kind: "c_body".to_string(),
        statements,
    });
}

#[derive(Debug)]
pub enum Statement {
    VariableDeclaration(VariableDeclaration),
    If(If),
    For(For),
    While(While),
    Return(Return),
    Switch(Switch),
    Expression(Expression),
}

impl Serialize for Statement {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            Statement::VariableDeclaration(x) => Serialize::serialize(x, serializer),
            Statement::If(x) => Serialize::serialize(x, serializer),
            Statement::For(x) => Serialize::serialize(x, serializer),
            Statement::While(x) => Serialize::serialize(x, serializer),
            Statement::Return(x) => Serialize::serialize(x, serializer),
            Statement::Switch(x) => Serialize::serialize(x, serializer),
            Statement::Expression(x) => Serialize::serialize(x, serializer),
        }
    }
}

pub fn parse_statement(lexer: &mut Lexer, typenames: &Vec<String>) -> Result<Statement, String> {
    let next = lexer.peek().unwrap();
    if (next.kind == "word" && is_type(&next.content.as_ref().unwrap(), typenames))
        || next.kind == "const"
    {
        return Ok(Statement::VariableDeclaration(parse_variable_declaration(
            lexer, typenames,
        )?));
    }

    match next.kind.as_str() {
        "if" => return Ok(Statement::If(parse_if(lexer, typenames)?)),
        "for" => return Ok(Statement::For(parse_for(lexer, typenames)?)),
        "while" => return Ok(Statement::While(parse_while(lexer, typenames)?)),
        "return" => return Ok(Statement::Return(parse_return(lexer, typenames)?)),
        "switch" => return Ok(Statement::Switch(parse_switch(lexer, typenames)?)),
        _ => {
            let expr = parse_expression(lexer, 0, typenames)?;
            expect(lexer, ";", Some("parsing statement"))?;
            return Ok(Statement::Expression(expr));
        }
    };
}

#[derive(Serialize, Debug)]
pub struct VariableDeclaration {
    kind: String,
    type_name: Type,
    forms: Vec<Form>,
    values: Vec<Expression>,
}

pub fn parse_variable_declaration(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
) -> Result<VariableDeclaration, String> {
    let type_name = parse_type(lexer, None)?;

    let mut forms = vec![parse_form(lexer, typenames)?];
    expect(lexer, "=", Some("variable declaration"))?;
    let mut values = vec![parse_expression(lexer, 0, typenames)?];

    while lexer.follows(",") {
        lexer.get();
        forms.push(parse_form(lexer, typenames)?);
        expect(lexer, "=", Some("variable declaration"))?;
        values.push(parse_expression(lexer, 0, typenames)?);
    }

    expect(lexer, ";", None)?;
    return Ok(VariableDeclaration {
        kind: "c_variable_declaration".to_string(),
        type_name,
        forms,
        values,
    });
}

#[derive(Serialize, Debug)]
pub struct Return {
    kind: String,
    expression: Option<Expression>,
}

pub fn parse_return(lexer: &mut Lexer, typenames: &Vec<String>) -> Result<Return, String> {
    expect(lexer, "return", None)?;
    if lexer.peek().unwrap().kind == ";" {
        lexer.get();
        return Ok(Return {
            kind: "c_return".to_string(),
            expression: None,
        });
    }
    let expression = parse_expression(lexer, 0, typenames)?;
    expect(lexer, ";", None)?;
    return Ok(Return {
        kind: "c_return".to_string(),
        expression: Some(expression),
    });
}

#[derive(Serialize, Debug)]
pub struct Form {
    kind: String,
    stars: String,
    name: String,
    indexes: Vec<Option<Expression>>,
}

pub fn parse_form(lexer: &mut Lexer, typenames: &Vec<String>) -> Result<Form, String> {
    // *argv[]
    // linechars[]
    // buf[SIZE * 2]
    let mut node = Form {
        kind: "c_form".to_string(),
        stars: String::new(),
        name: String::new(),
        indexes: vec![],
    };

    while lexer.follows("*") {
        node.stars += &lexer.get().unwrap().kind;
    }
    node.name = String::from(expect(lexer, "word", None).unwrap().content.unwrap());

    while lexer.follows("[") {
        &lexer.get().unwrap();
        let expr: Option<Expression>;
        if lexer.more() && lexer.peek().unwrap().kind != "]" {
            expr = Some(parse_expression(lexer, 0, typenames)?);
        } else {
            expr = None;
        }
        node.indexes.push(expr);
        expect(lexer, "]", None)?;
    }
    return Ok(node);
}

#[derive(Serialize, Debug)]
pub struct If {
    kind: String,
    condition: Expression,
    body: Body,
    else_body: Option<Body>,
}

pub fn parse_if(lexer: &mut Lexer, typenames: &Vec<String>) -> Result<If, String> {
    let condition;
    let body;
    let mut else_body = None;

    expect(lexer, "if", Some("if statement"))?;
    expect(lexer, "(", Some("if statement"))?;
    condition = parse_expression(lexer, 0, typenames)?;
    expect(lexer, ")", Some("if statement"))?;
    body = parse_body(lexer, typenames)?;
    if lexer.follows("else") {
        lexer.get();
        else_body = Some(parse_body(lexer, typenames)?);
    }
    return Ok(If {
        kind: "c_if".to_string(),
        condition,
        body,
        else_body,
    });
}

#[derive(Serialize, Debug)]
pub struct For {
    kind: String,
    init: ForInit,
    condition: Expression,
    action: Expression,
    body: Body,
}

#[derive(Debug)]
pub enum ForInit {
    Expression(Expression),
    LoopCounterDeclaration(LoopCounterDeclaration),
}

impl Serialize for ForInit {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            ForInit::Expression(x) => Serialize::serialize(x, serializer),
            ForInit::LoopCounterDeclaration(x) => Serialize::serialize(x, serializer),
        }
    }
}

#[derive(Serialize, Debug)]
pub struct LoopCounterDeclaration {
    kind: String,
    type_name: Type,
    name: Identifier,
    value: Expression,
}

pub fn parse_for(lexer: &mut Lexer, typenames: &Vec<String>) -> Result<For, String> {
    expect(lexer, "for", None)?;
    expect(lexer, "(", None)?;

    let init: ForInit;
    if lexer.peek().unwrap().kind == "word"
        && is_type(&lexer.peek().unwrap().content.as_ref().unwrap(), typenames)
    {
        let type_name = parse_type(lexer, None)?;
        let name = parse_identifier(lexer)?;
        expect(lexer, "=", None)?;
        let value = parse_expression(lexer, 0, typenames)?;
        init = ForInit::LoopCounterDeclaration(LoopCounterDeclaration {
            kind: "c_loop_counter_declaration".to_string(),
            type_name,
            name,
            value,
        });
    } else {
        init = ForInit::Expression(parse_expression(lexer, 0, typenames)?);
    }

    expect(lexer, ";", None)?;
    let condition = parse_expression(lexer, 0, typenames)?;
    expect(lexer, ";", None)?;
    let action = parse_expression(lexer, 0, typenames)?;
    expect(lexer, ")", None)?;
    let body = parse_body(lexer, typenames)?;

    return Ok(For {
        kind: "c_for".to_string(),
        init,
        condition,
        action,
        body,
    });
}

#[derive(Deserialize, Debug)]
pub enum SwitchCaseValue {
    Identifier(Identifier),
    Literal(Literal),
}

impl Serialize for SwitchCaseValue {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            SwitchCaseValue::Identifier(x) => Serialize::serialize(x, serializer),
            SwitchCaseValue::Literal(x) => Serialize::serialize(x, serializer),
        }
    }
}

#[derive(Serialize, Debug)]
pub struct SwitchCase {
    value: SwitchCaseValue,
    statements: Vec<Statement>,
}

#[derive(Serialize, Debug)]
pub struct Switch {
    kind: String,
    value: Expression,
    cases: Vec<SwitchCase>,
    default: Option<Vec<Statement>>,
}

pub fn parse_switch(lexer: &mut Lexer, typenames: &Vec<String>) -> Result<Switch, String> {
    expect(lexer, "switch", None)?;
    expect(lexer, "(", None)?;
    let value = parse_expression(lexer, 0, typenames)?;
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
            statements.push(parse_statement(lexer, typenames)?);
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
            def.push(parse_statement(lexer, typenames)?);
        }
        default = Some(def);
    }
    expect(lexer, "}", None)?;
    return Ok(Switch {
        kind: "c_switch".to_string(),
        value,
        cases,
        default,
    });
}

#[derive(Debug, Serialize)]
pub struct FunctionParameters {
    list: Vec<FunctionParameter>,
    variadic: bool,
}

#[derive(Debug, Serialize)]
pub struct FunctionDeclaration {
    kind: String,
    is_pub: bool,
    type_name: Type,
    form: Form,
    parameters: FunctionParameters,
    body: Body,
}

pub fn parse_function_declaration(
    lexer: &mut Lexer,
    is_pub: bool,
    type_name: Type,
    form: Form,
    typenames: &Vec<String>,
) -> Result<FunctionDeclaration, String> {
    let mut parameters: Vec<FunctionParameter> = vec![];
    let mut variadic = false;
    expect(lexer, "(", None)?;
    if !lexer.follows(")") {
        parameters.push(parse_function_parameter(lexer, typenames)?);
        while lexer.follows(",") {
            lexer.get();
            if lexer.follows("...") {
                lexer.get();
                variadic = true;
                break;
            }
            parameters.push(parse_function_parameter(lexer, typenames)?);
        }
    }
    expect(lexer, ")", None)?;
    let body = parse_body(lexer, typenames)?;
    return Ok(FunctionDeclaration {
        kind: "c_function_declaration".to_string(),
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

#[derive(Serialize, Debug)]
pub struct FunctionParameter {
    type_name: Type,
    forms: Vec<Form>,
}

pub fn parse_function_parameter(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
) -> Result<FunctionParameter, String> {
    let mut forms: Vec<Form> = vec![];
    let type_name = parse_type(lexer, None)?;
    forms.push(parse_form(lexer, typenames)?);
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
        forms.push(parse_form(lexer, typenames)?);
    }
    return Ok(FunctionParameter { type_name, forms });
}

#[derive(Serialize, Debug)]
pub struct Union {
    kind: String,
    form: Form,
    fields: Vec<UnionField>,
}

#[derive(Serialize, Debug)]
pub struct UnionField {
    type_name: Type,
    form: Form,
}

pub fn parse_union(lexer: &mut Lexer, typenames: &Vec<String>) -> Result<Union, String> {
    let mut fields: Vec<UnionField> = vec![];
    expect(lexer, "union", None)?;
    expect(lexer, "{", None)?;
    while !lexer.follows("}") {
        let type_name = parse_type(lexer, None)?;
        let form = parse_form(lexer, typenames)?;
        expect(lexer, ";", None)?;
        fields.push(UnionField { type_name, form });
    }
    expect(lexer, "}", None)?;
    let form = parse_form(lexer, typenames)?;
    expect(lexer, ";", None)?;
    return Ok(Union {
        kind: "c_union".to_string(),
        form,
        fields,
    });
}

#[derive(Serialize, Debug)]
pub struct ModuleVariable {
    kind: String,
    type_name: Type,
    form: Form,
    value: Expression,
}

#[derive(Debug)]
pub enum ModuleObject {
    ModuleVariable(ModuleVariable),
    Enum(Enum),
    FunctionDeclaration(FunctionDeclaration),
    Import(Import),
    Typedef(Typedef),
    CompatMacro(CompatMacro),
}

impl Serialize for ModuleObject {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            ModuleObject::ModuleVariable(x) => Serialize::serialize(x, serializer),
            ModuleObject::Enum(x) => Serialize::serialize(x, serializer),
            ModuleObject::FunctionDeclaration(x) => Serialize::serialize(x, serializer),
            ModuleObject::Import(x) => Serialize::serialize(x, serializer),
            ModuleObject::Typedef(x) => Serialize::serialize(x, serializer),
            ModuleObject::CompatMacro(x) => Serialize::serialize(x, serializer),
        }
    }
}

pub fn parse_module_object(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
) -> Result<ModuleObject, String> {
    let mut is_pub = false;
    if lexer.follows("pub") {
        lexer.get();
        is_pub = true;
    }
    if lexer.follows("enum") {
        return Ok(ModuleObject::Enum(parse_enum(lexer, is_pub)?));
    }
    let type_name = parse_type(lexer, None)?;
    let form = parse_form(lexer, typenames)?;
    if lexer.peek().unwrap().kind == "(" {
        return Ok(ModuleObject::FunctionDeclaration(
            parse_function_declaration(lexer, is_pub, type_name, form, typenames)?,
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
        let value = parse_expression(lexer, 0, typenames)?;
        expect(lexer, ";", Some("module variable declaration"))?;
        let var = ModuleVariable {
            kind: "c_module_variable".to_string(),
            type_name,
            form,
            value,
        };
        return Ok(ModuleObject::ModuleVariable(var));
    }
    return Err("unexpected input".to_string());
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Import {
    kind: String,
    path: String,
}

pub fn parse_import(lexer: &mut Lexer) -> Result<Import, String> {
    expect(lexer, "import", None)?;
    let path = expect(lexer, "string", None).unwrap().content.unwrap();

    return Ok(Import {
        kind: "c_import".to_string(),
        path,
    });
}

#[derive(Serialize, Deserialize, Debug)]
pub struct CompatMacro {
    kind: String,
    name: String,
    value: String,
}

pub fn parse_compat_macro(lexer: &mut Lexer) -> Result<CompatMacro, String> {
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
        kind: "c_compat_macro".to_string(),
        name: name[1..].to_string(),
        value: value.to_string(),
    });
}

#[derive(Debug)]
pub enum TypedefTarget {
    AnonymousStruct(AnonymousStruct),
    Type(Type),
}

impl Serialize for TypedefTarget {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            TypedefTarget::AnonymousStruct(x) => Serialize::serialize(x, serializer),
            TypedefTarget::Type(x) => Serialize::serialize(x, serializer),
        }
    }
}

#[derive(Serialize, Debug)]
pub struct Typedef {
    kind: String,
    type_name: TypedefTarget,
    form: TypedefForm,
}

pub fn parse_typedef(lexer: &mut Lexer, typenames: &Vec<String>) -> Result<Typedef, String> {
    // typedef void *f(int a)[20];
    // typedef foo bar;
    // typedef struct foo foo_t;
    expect(lexer, "typedef", None)?;
    let type_name;
    if lexer.follows("{") {
        type_name = TypedefTarget::AnonymousStruct(parse_anonymous_struct(lexer, typenames)?);
    } else {
        type_name = TypedefTarget::Type(parse_type(lexer, Some("typedef"))?);
    }
    let form = parse_typedef_form(lexer)?;
    expect(lexer, ";", Some("typedef"))?;
    return Ok(Typedef {
        kind: "c_typedef".to_string(),
        type_name,
        form,
    });
}

#[derive(Serialize, Debug)]
pub struct TypedefForm {
    stars: String,
    params: Option<AnonymousParameters>,
    size: usize,
    alias: Identifier,
}

pub fn parse_typedef_form(lexer: &mut Lexer) -> Result<TypedefForm, String> {
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

#[derive(Debug)]
pub enum StructEntry {
    StructFieldlist(StructFieldlist),
    Union(Union),
}

impl Serialize for StructEntry {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            StructEntry::Union(x) => Serialize::serialize(x, serializer),
            StructEntry::StructFieldlist(x) => Serialize::serialize(x, serializer),
        }
    }
}

#[derive(Serialize, Debug)]
pub struct AnonymousStruct {
    kind: String,
    fieldlists: Vec<StructEntry>,
}

pub fn parse_anonymous_struct(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
) -> Result<AnonymousStruct, String> {
    let mut fieldlists: Vec<StructEntry> = vec![];
    expect(lexer, "{", Some("struct type definition"))?;
    while lexer.more() && lexer.peek().unwrap().kind != "}" {
        if lexer.peek().unwrap().kind == "union" {
            fieldlists.push(StructEntry::Union(parse_union(lexer, typenames)?));
        } else {
            fieldlists.push(StructEntry::StructFieldlist(parse_struct_fieldlist(
                lexer, typenames,
            )?));
        }
    }
    expect(lexer, "}", None)?;

    return Ok(AnonymousStruct {
        kind: "c_anonymous_struct".to_string(),
        fieldlists,
    });
}

#[derive(Serialize, Debug)]
pub struct StructFieldlist {
    kind: String,
    type_name: Type,
    forms: Vec<Form>,
}

pub fn parse_struct_fieldlist(
    lexer: &mut Lexer,
    typenames: &Vec<String>,
) -> Result<StructFieldlist, String> {
    if lexer.follows("struct") {
        return Err("can't parse nested structs, please consider a typedef".to_string());
    }

    let type_name = parse_type(lexer, None)?;

    let mut forms: Vec<Form> = vec![];
    forms.push(parse_form(lexer, typenames)?);
    while lexer.follows(",") {
        lexer.get();
        forms.push(parse_form(lexer, typenames)?);
    }

    expect(lexer, ";", None)?;
    return Ok(StructFieldlist {
        kind: "c_struct_fieldlist".to_string(),
        type_name,
        forms,
    });
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

pub fn get_typename(lexer: &mut Lexer) -> Result<String, String> {
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

pub fn get_file_typenames(path: &str) -> Result<Vec<String>, String> {
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

pub fn get_module(name: &str) -> Result<Module, String> {
    let mut module_path = String::new();
    if name.ends_with(".c") {
        module_path += name;
    } else {
        module_path += format!("lib/{}.c", name).as_str();
    };

    let k = std::fs::metadata(module_path.as_str());
    if k.is_err() {
        return Err(format!("can't find module '{}'", name));
    }

    let types = get_file_typenames(module_path.as_str())?;
    let mut lexer = new(module_path.as_str());
    let r = parse_module(&mut lexer, types);
    if r.is_err() {
        let next = lexer.peek().unwrap();
        let wher = format!("{}: {}", module_path, lexer.peek().unwrap().pos);
        let what = r.err().unwrap();
        return Err(format!("{}: {}: {}...", wher, what, token_to_string(next)));
    }
    return r;
}

pub fn token_to_string(token: &Token) -> String {
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

#[derive(Serialize)]
pub struct Module {
    kind: String,
    elements: Vec<ModuleObject>,
}

pub fn parse_module(lexer: &mut Lexer, typenames: Vec<String>) -> Result<Module, String> {
    let mut elements: Vec<ModuleObject> = vec![];
    let mut types = typenames.clone();
    while lexer.more() {
        match lexer.peek().unwrap().kind.as_str() {
            "import" => {
                let import = parse_import(lexer)?;
                let p = import.path.clone();
                elements.push(ModuleObject::Import(import));

                let module = get_module(p.as_str())?;
                for element in module.elements {
                    match element {
                        ModuleObject::Typedef(t) => {
                            types.push(t.form.alias.name);
                        }
                        _ => {}
                    }
                }
            }
            "typedef" => {
                elements.push(ModuleObject::Typedef(parse_typedef(lexer, &types)?));
            }
            "macro" => {
                elements.push(ModuleObject::CompatMacro(parse_compat_macro(lexer)?));
            }
            _ => {
                elements.push(parse_module_object(lexer, &types)?);
            }
        }
    }
    return Ok(Module {
        kind: "c_module".to_string(),
        elements,
    });
}
