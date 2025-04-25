use crate::buf::Pos;

#[derive(Debug, Clone)]
pub struct Module {
    pub elements: Vec<ModElem>,
}

// Elements than can be at module level.
#[derive(Debug, Clone)]
pub enum ModElem {
    Macro(Macro),
    Import(ImportNode),
    Enum(Enum),
    StructAliasTypedef(StructAlias),
    Typedef(Typedef),
    StructTypedef(StructTypedef),
    ModuleVariable(VariableDeclaration),
    FunctionDeclaration(FunctionDeclaration),
}

// Elements than can be a part of a function body.
#[derive(Debug, Clone)]
pub enum Statement {
    Break,
    Continue,
    VariableDeclaration(VariableDeclaration),
    If(If),
    For(For),
    While(While),
    Panic(Panic),
    Return(Return),
    Switch(Switch),
    Expression(Expression),
}

// Elements that can participate in expressions.
#[derive(Debug, Clone)]
pub enum Expression {
    ArrayIndex(ArrayIndex),
    BinaryOp(BinaryOp),
    FieldAccess(FieldAccess),
    Cast(Cast),
    CompositeLiteral(CompositeLiteral),
    FunctionCall(FunctionCall),
    Identifier(Identifier),
    Literal(Literal),
    NsName(NsName),
    PostfixOperator(PostfixOperator),
    PrefixOperator(PrefixOperator),
    Sizeof(Sizeof),
}

// #import foo
#[derive(Debug, Clone)]
pub struct ImportNode {
    pub specified_path: String,
    pub pos: Pos,
}

// typedef struct tm tm_t;
#[derive(Debug, Clone)]
pub struct StructAlias {
    pub pos: Pos,
    pub is_pub: bool,
    pub struct_name: String,
    pub type_alias: String,
}

#[derive(Debug, Clone)]
pub struct Macro {
    pub name: String,
    pub value: String,
    pub pos: Pos,
}

// enum { A = 1, B, ... }
#[derive(Debug, Clone)]
pub struct Enum {
    pub is_pub: bool,
    pub members: Vec<EnumEntry>,
    pub pos: Pos,
}

#[derive(Debug, Clone)]
pub struct EnumEntry {
    pub id: Identifier,
    pub value: Option<Expression>,
    pub pos: Pos,
}

#[derive(Debug, Clone)]
pub struct FunctionDeclaration {
    pub is_pub: bool,
    pub type_name: Typename,
    pub form: Form,
    pub parameters: FunctionParameters,
    pub body: Body,
    pub pos: Pos,
}

#[derive(Debug, Clone)]
pub struct Typedef {
    pub pos: Pos,
    pub is_pub: bool,
    pub alias: Identifier,
    pub type_name: Typename,
    pub dereference_count: usize,
    pub array_size: usize,
    pub function_parameters: Option<AnonymousParameters>,
}

#[derive(Debug, Clone)]
pub struct StructTypedef {
    pub pos: Pos,
    pub is_pub: bool,
    pub fields: Vec<StructEntry>,
    pub name: Identifier,
}

// #[derive(Debug, Clone)]
// pub struct FuncTypedef {
//     pub is_pub: bool,
//     pub return_type: Typename,
//     pub name: String,
//     pub params: AnonymousParameters,
// }

#[derive(Debug, Clone)]
pub struct Typename {
    pub is_const: bool,
    pub name: NsName,
}

#[derive(Debug, Clone)]
pub struct NsName {
    pub namespace: String,
    pub name: String,
    pub pos: Pos,
}

#[derive(Debug, Clone)]
pub struct AnonymousTypeform {
    pub type_name: Typename,
    pub ops: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct CompositeLiteral {
    pub entries: Vec<CompositeLiteralEntry>,
}

#[derive(Debug, Clone)]
pub struct CompositeLiteralEntry {
    pub is_index: bool,
    pub key: Option<Expression>,
    pub value: Expression,
}

// ...[...]
#[derive(Debug, Clone)]
pub struct ArrayIndex {
    pub array: Box<Expression>,
    pub index: Box<Expression>,
}

// ...(...)
#[derive(Debug, Clone)]
pub struct FunctionCall {
    pub function: Box<Expression>,
    pub arguments: Vec<Expression>,
}

#[derive(Debug, Clone)]
pub struct BinaryOp {
    pub op: String,
    pub a: Box<Expression>,
    pub b: Box<Expression>,
}

#[derive(Debug, Clone)]
pub struct FieldAccess {
    pub op: String,
    pub target: Box<Expression>,
    pub field_name: Identifier,
}

#[derive(Debug, Clone)]
pub struct Cast {
    pub type_name: AnonymousTypeform,
    pub operand: Box<Expression>,
}

#[derive(Debug, Clone)]
pub struct PostfixOperator {
    pub operator: String,
    pub operand: Box<Expression>,
}

#[derive(Debug, Clone)]
pub struct PrefixOperator {
    pub operator: String,
    pub operand: Box<Expression>,
}

#[derive(Debug, Clone)]
pub struct Sizeof {
    pub argument: Box<SizeofArgument>,
}

#[derive(Debug, Clone)]
pub enum SizeofArgument {
    Typename(Typename),
    Expression(Expression),
}

#[derive(Debug, Clone)]
pub struct Body {
    pub statements: Vec<Statement>,
}

#[derive(Debug, Clone)]
pub struct Identifier {
    pub name: String,
    pub pos: Pos,
}

#[derive(Debug, Clone)]
pub struct VariableDeclaration {
    pub type_name: Typename,
    pub form: Form,
    pub value: Option<Expression>,
    pub pos: Pos,
}

#[derive(Debug, Clone)]
pub struct Panic {
    pub arguments: Vec<Expression>,
    pub pos: String,
}

#[derive(Debug, Clone)]
pub struct If {
    pub condition: Expression,
    pub body: Body,
    pub else_body: Option<Body>,
}

#[derive(Debug, Clone)]
pub struct For {
    pub init: Option<ForInit>,
    pub condition: Option<Expression>,
    pub action: Option<Expression>,
    pub body: Body,
}

#[derive(Debug, Clone)]
pub struct While {
    pub condition: Expression,
    pub body: Body,
}

#[derive(Debug, Clone)]
pub struct Return {
    pub expression: Option<Expression>,
}

#[derive(Debug, Clone)]
pub struct Switch {
    pub is_str: bool,
    pub value: Expression,
    pub cases: Vec<SwitchCase>,
    pub default_case: Option<Body>,
}

#[derive(Debug, Clone)]
pub struct Form {
    pub hops: usize,
    pub name: String,
    pub indexes: Vec<Option<Expression>>,
    pub pos: Pos,
}

#[derive(Debug, Clone)]
pub enum ForInit {
    Expression(Expression),
    LoopCounterDeclaration {
        type_name: Typename,
        form: Form,
        value: Expression,
    },
}

#[derive(Debug, Clone)]
pub enum SwitchCaseValue {
    Identifier(NsName),
    Literal(Literal),
}

#[derive(Debug, Clone)]
pub enum Literal {
    Char(String),
    String(String),
    Number(String),
    Null,
}

#[derive(Debug, Clone)]
pub struct SwitchCase {
    pub values: Vec<SwitchCaseValue>,
    pub body: Body,
}

#[derive(Debug, Clone)]
pub struct FunctionParameters {
    pub list: Vec<TypeAndForms>,
    pub variadic: bool,
}

#[derive(Debug, Clone)]
pub struct TypeAndForms {
    pub type_name: Typename,
    pub forms: Vec<Form>,
    pub pos: Pos,
}

#[derive(Debug, Clone)]
pub struct Union {
    pub form: Form,
    pub fields: Vec<UnionField>,
}

#[derive(Debug, Clone)]
pub struct UnionField {
    pub type_name: Typename,
    pub form: Form,
}

#[derive(Debug, Clone)]
pub struct AnonymousParameters {
    pub ellipsis: bool,
    pub forms: Vec<AnonymousTypeform>,
}

#[derive(Debug, Clone)]
pub enum StructEntry {
    Plain(TypeAndForms),
    Union(Union),
}
