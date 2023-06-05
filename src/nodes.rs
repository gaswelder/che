use crate::nodes_c::*;

// Che

#[derive(Clone)]
pub struct Module {
    pub id: String,
    pub elements: Vec<ModuleObject>,
    pub source_path: String,
}

#[derive(Debug, Clone)]
pub enum ModuleObject {
    CompatMacro(CompatMacro),
    Import { path: String },
    Enum(Enum),
    Typedef(Typedef),
    StructTypedef(StructTypedef),
    FuncTypedef(FuncTypedef),
    ModuleVariable(ModuleVariable),
    FunctionDeclaration(FunctionDeclaration),
}

#[derive(Debug, Clone)]
pub struct FunctionDeclaration {
    pub is_pub: bool,
    pub type_name: Typename,
    pub form: Form,
    pub parameters: FunctionParameters,
    pub body: Body,
}

// C

#[derive(Debug, Clone)]
pub struct CModule {
    pub id: String,
    pub elements: Vec<CModuleObject>,
    pub link: Vec<String>,
}

#[derive(Debug, Clone)]
pub enum CModuleObject {
    Include(String),
    Macro {
        name: String,
        value: String,
    },
    EnumDefinition {
        members: Vec<CEnumItem>,
        is_hidden: bool,
    },
    Typedef {
        is_pub: bool,
        type_name: CTypename,
        form: CTypedefForm,
    },
    FuncTypedef {
        is_pub: bool,
        return_type: CTypename,
        name: String,
        params: CAnonymousParameters,
    },
    StructForwardDeclaration(String),
    StructDefinition {
        name: String,
        fields: Vec<CompatStructEntry>,
        is_pub: bool,
    },
    ModuleVariable {
        type_name: CTypename,
        form: CForm,
        value: CExpression,
    },
    FunctionForwardDeclaration {
        is_static: bool,
        type_name: CTypename,
        form: CForm,
        parameters: CompatFunctionParameters,
    },
    FunctionDefinition {
        is_static: bool,
        type_name: CTypename,
        form: CForm,
        parameters: CompatFunctionParameters,
        body: CBody,
    },
    Split {
        text: String,
    },
}

#[derive(Debug, Clone)]
pub struct CompatFunctionParameters {
    pub list: Vec<CompatFunctionParameter>,
    pub variadic: bool,
}

#[derive(Debug, Clone)]
pub struct CompatFunctionParameter {
    pub type_name: CTypename,
    pub form: CForm,
}

#[derive(Debug, Clone)]
pub enum CompatStructEntry {
    CompatStructField { type_name: CTypename, form: CForm },
    Union(CUnion),
}

// Both

#[derive(Debug, Clone)]
pub struct Typedef {
    pub is_pub: bool,
    pub type_name: Typename,
    pub form: TypedefForm,
}

#[derive(Debug, Clone)]
pub struct StructTypedef {
    pub is_pub: bool,
    pub fields: Vec<StructEntry>,
    pub name: String,
}

#[derive(Debug, Clone)]
pub struct FuncTypedef {
    pub is_pub: bool,
    pub return_type: Typename,
    pub name: String,
    pub params: AnonymousParameters,
}

#[derive(Debug, Clone)]
pub struct Enum {
    pub is_pub: bool,
    pub members: Vec<EnumItem>,
}

#[derive(Debug, Clone)]
pub struct EnumItem {
    pub id: String,
    pub value: Option<Expression>,
}

#[derive(Debug, Clone)]
pub struct CEnumItem {
    pub id: String,
    pub value: Option<CExpression>,
}

#[derive(Debug, Clone)]
pub struct Typename {
    pub is_const: bool,
    pub name: String,
}

#[derive(Debug, Clone)]
pub struct AnonymousTypeform {
    pub type_name: Typename,
    pub ops: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct CAnonymousTypeform {
    pub type_name: CTypename,
    pub ops: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct Literal {
    pub type_name: String,
    pub value: String,
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

#[derive(Debug, Clone)]
pub struct CCompositeLiteral {
    pub entries: Vec<CCompositeLiteralEntry>,
}

#[derive(Debug, Clone)]
pub struct CCompositeLiteralEntry {
    pub is_index: bool,
    pub key: Option<CExpression>,
    pub value: CExpression,
}

#[derive(Debug, Clone)]
pub enum Expression {
    CompositeLiteral(CompositeLiteral),
    Literal(Literal),
    Identifier(String),
    BinaryOp {
        op: String,
        a: Box<Expression>,
        b: Box<Expression>,
    },
    PrefixOperator {
        operator: String,
        operand: Box<Expression>,
    },
    PostfixOperator {
        operator: String,
        operand: Box<Expression>,
    },
    Cast {
        type_name: AnonymousTypeform,
        operand: Box<Expression>,
    },
    FunctionCall {
        function: Box<Expression>,
        arguments: Vec<Expression>,
    },
    Sizeof {
        argument: Box<SizeofArgument>,
    },
    ArrayIndex {
        array: Box<Expression>,
        index: Box<Expression>,
    },
}

#[derive(Debug, Clone)]
pub enum CExpression {
    Literal(Literal),
    CompositeLiteral(CCompositeLiteral),
    Identifier(String),
    BinaryOp {
        op: String,
        a: Box<CExpression>,
        b: Box<CExpression>,
    },
    PrefixOperator {
        operator: String,
        operand: Box<CExpression>,
    },
    PostfixOperator {
        operator: String,
        operand: Box<CExpression>,
    },
    Cast {
        type_name: CAnonymousTypeform,
        operand: Box<CExpression>,
    },
    FunctionCall {
        function: Box<CExpression>,
        arguments: Vec<CExpression>,
    },
    Sizeof {
        argument: Box<CSizeofArgument>,
    },
    ArrayIndex {
        array: Box<CExpression>,
        index: Box<CExpression>,
    },
}

#[derive(Debug, Clone)]
pub struct StructLiteralMember {
    pub name: String,
    pub value: Expression,
}

#[derive(Debug, Clone)]
pub enum SizeofArgument {
    Typename(Typename),
    Expression(Expression),
}

#[derive(Debug, Clone)]
pub enum CSizeofArgument {
    Typename(CTypename),
    Expression(CExpression),
}

#[derive(Debug, Clone)]
pub struct Body {
    pub statements: Vec<Statement>,
}

#[derive(Debug, Clone)]
pub struct CBody {
    pub statements: Vec<CStatement>,
}

#[derive(Debug, Clone)]
pub enum Statement {
    VariableDeclaration {
        type_name: Typename,
        forms: Vec<Form>,
        values: Vec<Option<Expression>>,
    },
    If {
        condition: Expression,
        body: Body,
        else_body: Option<Body>,
    },
    For {
        init: ForInit,
        condition: Expression,
        action: Expression,
        body: Body,
    },
    While {
        condition: Expression,
        body: Body,
    },
    Return {
        expression: Option<Expression>,
    },
    Switch {
        value: Expression,
        cases: Vec<SwitchCase>,
        default: Option<Body>,
    },
    Expression(Expression),
}

#[derive(Debug, Clone)]
pub enum CStatement {
    VariableDeclaration {
        type_name: CTypename,
        forms: Vec<CForm>,
        values: Vec<Option<CExpression>>,
    },
    If {
        condition: CExpression,
        body: CBody,
        else_body: Option<CBody>,
    },
    For {
        init: CForInit,
        condition: CExpression,
        action: CExpression,
        body: CBody,
    },
    While {
        condition: CExpression,
        body: CBody,
    },
    Return {
        expression: Option<CExpression>,
    },
    Switch {
        value: CExpression,
        cases: Vec<CSwitchCase>,
        default: Option<CBody>,
    },
    Expression(CExpression),
}

#[derive(Debug, Clone)]
pub struct Form {
    pub stars: String,
    pub name: String,
    pub indexes: Vec<Option<Expression>>,
}

#[derive(Debug, Clone)]
pub struct CForm {
    pub stars: String,
    pub name: String,
    pub indexes: Vec<Option<CExpression>>,
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
pub enum CForInit {
    Expression(CExpression),
    LoopCounterDeclaration {
        type_name: CTypename,
        form: CForm,
        value: CExpression,
    },
}

#[derive(Debug, Clone)]
pub enum SwitchCaseValue {
    Identifier(String),
    Literal(Literal),
}

#[derive(Debug, Clone)]
pub struct SwitchCase {
    pub value: SwitchCaseValue,
    pub body: Body,
}

#[derive(Debug, Clone)]
pub struct CSwitchCase {
    pub value: SwitchCaseValue,
    pub body: CBody,
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
pub struct CUnion {
    pub form: CForm,
    pub fields: Vec<CUnionField>,
}

#[derive(Debug, Clone)]
pub struct CUnionField {
    pub type_name: CTypename,
    pub form: CForm,
}

#[derive(Debug, Clone)]
pub struct ModuleVariable {
    pub type_name: Typename,
    pub form: Form,
    pub value: Expression,
}

#[derive(Debug, Clone)]
pub struct TypedefForm {
    pub stars: String,
    pub params: Option<AnonymousParameters>,
    pub size: usize,
    pub alias: String,
}

#[derive(Debug, Clone)]
pub struct CTypedefForm {
    pub stars: String,
    pub params: Option<CAnonymousParameters>,
    pub size: usize,
    pub alias: String,
}

#[derive(Debug, Clone)]
pub struct AnonymousParameters {
    pub ellipsis: bool,
    pub forms: Vec<AnonymousTypeform>,
}

#[derive(Debug, Clone)]
pub struct CAnonymousParameters {
    pub ellipsis: bool,
    pub forms: Vec<CAnonymousTypeform>,
}

#[derive(Debug, Clone)]
pub enum StructEntry {
    Plain(TypeAndForms),
    Union(Union),
}
