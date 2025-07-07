use crate::buf::Pos;

#[derive(Debug, Clone)]
pub struct Module {
    pub elements: Vec<ModElem>,
    pub exports: Exports,
}

#[derive(Debug, Clone)]
pub struct Exports {
    pub consts: Vec<EnumEntry>,
    pub fns: Vec<FuncDecl>,
    pub types: Vec<String>,
    pub structs: Vec<StructTypedef>,
}

pub fn exports_has(e: &Exports, name: &str) -> bool {
    e.consts.iter().any(|x| x.name == name)
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
    ModVar(VarDecl),
    FuncDecl(FuncDecl),
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
    Expression(Expr),
}

// Elements that can participate in expressions.
#[derive(Debug, Clone)]
pub enum Expr {
    ArrIndex(ArrayIndex),
    BinaryOp(BinaryOp),
    FieldAccess(FieldAccess),
    Cast(Cast),
    CompositeLiteral(CompLiteral),
    Call(Call),
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
    pub name: String,
    pub val: Option<Expr>,
    // pub pos: Pos,
}

// pub? int *f(int a, b; double *foo) { ... }
#[derive(Debug, Clone)]
pub struct FuncDecl {
    pub pos: Pos,
    pub ispub: bool,
    pub typename: Typename,
    pub form: Form,
    pub params: FuncParams,
    pub body: Body,
}

#[derive(Debug, Clone)]
pub struct Typedef {
    pub ispub: bool,
    pub alias: String,
    pub typename: Typename,
    pub derefs: usize,
    pub array_size: usize,
    pub func_params: Option<AnonymousParameters>,
}

// typedef { int x, y; double *f; } foo_t
#[derive(Debug, Clone)]
pub struct StructTypedef {
    pub ispub: bool,
    pub entries: Vec<StructEntry>,
    pub name: String,
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

// strings.casecmp
#[derive(Debug, Clone)]
pub struct NsName {
    pub pos: Pos,
    pub ns: String,
    pub name: String,
}

#[derive(Debug, Clone)]
pub struct CompLiteral {
    pub entries: Vec<CompositeLiteralEntry>,
}

#[derive(Debug, Clone)]
pub struct CompositeLiteralEntry {
    pub is_index: bool,
    pub key: Option<Expr>,
    pub value: Expr,
}

// ...[...]
#[derive(Debug, Clone)]
pub struct ArrayIndex {
    pub array: Box<Expr>,
    pub index: Box<Expr>,
}

// ...(...)
#[derive(Debug, Clone)]
pub struct Call {
    pub pos: Pos,
    pub func: Box<Expr>,
    pub args: Vec<Expr>,
}

// a <op> b
#[derive(Debug, Clone)]
pub struct BinaryOp {
    pub pos: Pos,
    pub op: String,
    pub a: Box<Expr>,
    pub b: Box<Expr>,
}

// a->b, a.b
#[derive(Debug, Clone)]
pub struct FieldAccess {
    pub pos: Pos,
    pub op: String,
    pub target: Box<Expr>,
    pub field_name: String,
}

#[derive(Debug, Clone)]
pub struct Cast {
    pub typeform: BareTypeform,
    pub operand: Box<Expr>,
}

#[derive(Debug, Clone)]
pub struct PostfixOp {
    pub operator: String,
    pub operand: Box<Expr>,
}

#[derive(Debug, Clone)]
pub struct PrefixOp {
    pub pos: Pos,
    pub operator: String,
    pub operand: Box<Expr>,
}

#[derive(Debug, Clone)]
pub struct Sizeof {
    pub arg: Box<SizeofArg>,
}

#[derive(Debug, Clone)]
pub enum SizeofArg {
    Typename(BareTypeform),
    Expr(Expr),
}

#[derive(Debug, Clone)]
pub struct Body {
    pub statements: Vec<Statement>,
}

// int foo = 1;
// int foo;
#[derive(Debug, Clone)]
pub struct VarDecl {
    pub pos: Pos,
    pub typename: Typename,
    pub form: Form,
    pub value: Option<Expr>,
}

#[derive(Debug, Clone)]
pub struct If {
    pub condition: Expr,
    pub body: Body,
    pub else_body: Option<Body>,
}

#[derive(Debug, Clone)]
pub struct For {
    pub init: Option<ForInit>,
    pub condition: Option<Expr>,
    pub action: Option<Expr>,
    pub body: Body,
}

#[derive(Debug, Clone)]
pub struct While {
    pub cond: Expr,
    pub body: Body,
}

#[derive(Debug, Clone)]
pub struct Return {
    pub expression: Option<Expr>,
}

#[derive(Debug, Clone)]
pub struct Switch {
    pub is_str: bool,
    pub value: Expr,
    pub cases: Vec<SwitchCase>,
    pub default_case: Option<Body>,
}

// *foo[]
#[derive(Debug, Clone)]
pub struct Form {
    pub pos: Pos,
    pub name: String,
    pub hops: usize,                // the number of dereferences, stars on the left
    pub indexes: Vec<Option<Expr>>, // brackets on the right
}

#[derive(Debug, Clone)]
pub struct BareTypeform {
    pub typename: Typename,
    pub hops: usize,
}

#[derive(Debug, Clone)]
pub enum ForInit {
    Expr(Expr),
    DeclLoopCounter {
        type_name: Typename,
        form: Form,
        value: Expr,
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
    pub ellipsis: bool,
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
    pub forms: Vec<BareTypeform>,
}

pub fn is_op(e: &Expr) -> Option<String> {
    match e {
        Expr::BinaryOp(x) => Some(String::from(&x.op)),
        Expr::PostfixOperator { .. } => Some(String::from("prefix")),
        Expr::PrefixOperator { .. } => Some(String::from("prefix")),
        _ => None,
    }
}

pub fn is_binary_op(a: &Expr) -> Option<&String> {
    match a {
        Expr::BinaryOp(x) => Some(&x.op),
        _ => None,
    }
}

// Returns true if the given expression is an identifier with the given name.
pub fn is_ident(x: &Expr, name: &str) -> bool {
    match x {
        Expr::NsName(ns_name) => ns_name.ns == "" && ns_name.name == name,
        _ => false,
    }
}

pub fn is_void(t: &Typename) -> bool {
    return t.name.ns == "" && t.name.name == "void";
}
