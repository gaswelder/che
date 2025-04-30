use crate::buf::Pos;

#[derive(Debug, Clone)]
pub struct Module {
    pub elements: Vec<ModElem>,
    pub exports: Exports,
}

#[derive(Debug, Clone)]
pub struct Exports {
    pub consts: Vec<EnumEntry>,
    pub fns: Vec<DeclFunc>,
    pub types: Vec<String>,
    // pub structs: Vec<StructTypedef>,
}

pub fn exports_has(e: &Exports, name: &str) -> bool {
    e.consts.iter().any(|x| x.id == name)
        || e.fns.iter().any(|x| x.form.name == name)
        || e.types.iter().any(|x| x == name)
}

// Elements than can be at module level.
#[derive(Debug, Clone)]
pub enum ModElem {
    Macro(Macro),
    Enum(Enum),
    StructAlias(StructAlias),
    Typedef(Typedef),
    StructTypedef(StructTypedef),
    ModuleVariable(VarDecl),
    DeclFunc(DeclFunc),
}

// Elements than can be a part of a function body.
#[derive(Debug, Clone)]
pub enum Statement {
    Break,
    Continue,
    VarDecl(VarDecl),
    If(If),
    For(For),
    While(While),
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
    Identifier(Ident),
    Literal(Literal),
    NsName(NsName),
    PostfixOperator(PostfixOp),
    PrefixOperator(PrefixOp),
    Sizeof(Sizeof),
}

// pub? typedef struct tm tm_t;
// Compatibility hatch to expose an existing struct as a type.
#[derive(Debug, Clone)]
pub struct StructAlias {
    pub pos: Pos,
    pub ispub: bool,
    pub structname: String,
    pub typename: String,
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
    pub entries: Vec<EnumEntry>,
    pub pos: Pos,
}

#[derive(Debug, Clone)]
pub struct EnumEntry {
    pub id: String,
    pub value: Option<Expression>,
    // pub pos: Pos,
}

// pub? int *f(int a, b; double *foo) { ... }
#[derive(Debug, Clone)]
pub struct DeclFunc {
    pub pos: Pos,
    pub ispub: bool,
    pub typename: Typename,
    pub form: Form,
    pub params: FuncParams,
    pub body: Body,
}

#[derive(Debug, Clone)]
pub struct Typedef {
    pub pos: Pos,
    pub is_pub: bool,
    pub alias: Ident,
    pub type_name: Typename,
    pub dereference_count: usize,
    pub array_size: usize,
    pub function_parameters: Option<AnonymousParameters>,
}

// typedef { int x, y; double *f; } foo_t
#[derive(Debug, Clone)]
pub struct StructTypedef {
    pub pos: Pos,
    pub ispub: bool,
    pub fields: Vec<StructEntry>,
    pub name: Ident,
}

#[derive(Debug, Clone)]
pub enum StructEntry {
    Plain(TypeAndForms),
    Union(Union),
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
    pub func: Box<Expression>,
    pub args: Vec<Expression>,
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
    pub field_name: Ident,
}

#[derive(Debug, Clone)]
pub struct Cast {
    pub type_name: AnonymousTypeform,
    pub operand: Box<Expression>,
}

#[derive(Debug, Clone)]
pub struct PostfixOp {
    pub operator: String,
    pub operand: Box<Expression>,
}

#[derive(Debug, Clone)]
pub struct PrefixOp {
    pub operator: String,
    pub operand: Box<Expression>,
}

#[derive(Debug, Clone)]
pub struct Sizeof {
    pub argument: Box<SizeofArg>,
}

#[derive(Debug, Clone)]
pub enum SizeofArg {
    Typename(Typename),
    Expr(Expression),
}

#[derive(Debug, Clone)]
pub struct Body {
    pub statements: Vec<Statement>,
}

#[derive(Debug, Clone)]
pub struct Ident {
    pub name: String,
    pub pos: Pos,
}

#[derive(Debug, Clone)]
pub struct VarDecl {
    pub type_name: Typename,
    pub form: Form,
    pub value: Option<Expression>,
    pub pos: Pos,
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
    pub cond: Expression,
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

// *foo[]
#[derive(Debug, Clone)]
pub struct Form {
    pub pos: Pos,
    pub name: String,
    pub hops: usize, // the number of dereferences, stars on the left
    pub indexes: Vec<Option<Expression>>, // brackets on the right
}

#[derive(Debug, Clone)]
pub enum ForInit {
    Expr(Expression),
    DeclLoopCounter {
        type_name: Typename,
        form: Form,
        value: Expression,
    },
}

#[derive(Debug, Clone)]
pub enum SwitchCaseValue {
    Ident(NsName),
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
pub struct FuncParams {
    pub list: Vec<TypeAndForms>,
    pub variadic: bool,
}

#[derive(Debug, Clone)]
pub struct TypeAndForms {
    pub typename: Typename,
    pub forms: Vec<Form>,
    // pub pos: Pos,
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

pub fn is_op(e: &Expression) -> Option<String> {
    match e {
        Expression::BinaryOp(x) => Some(String::from(&x.op)),
        Expression::PostfixOperator { .. } => Some(String::from("prefix")),
        Expression::PrefixOperator { .. } => Some(String::from("prefix")),
        _ => None,
    }
}

pub fn is_binary_op(a: &Expression) -> Option<&String> {
    match a {
        Expression::BinaryOp(x) => Some(&x.op),
        _ => None,
    }
}

// Returns true if the given expression is an identifier with the given name.
pub fn is_ident(x: &Expression, name: &str) -> bool {
    match x {
        Expression::NsName(ns_name) => ns_name.namespace == "" && ns_name.name == name,
        Expression::Identifier(x) => x.name == name,
        _ => false,
    }
}

pub fn is_void(t: &Typename) -> bool {
    return t.name.namespace == "" && t.name.name == "void";
}
