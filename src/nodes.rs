#[derive(Debug, Clone)]
pub struct Module {
    pub elements: Vec<ModuleObject>,
}

#[derive(Debug, Clone)]
pub struct ModuleRef {
    pub source_path: String,
    pub id: String,
}

#[derive(Debug, Clone)]
pub enum ModuleObject {
    Macro {
        name: String,
        value: String,
        pos: String,
    },
    Import(ImportNode),
    Enum {
        is_pub: bool,
        members: Vec<EnumItem>,
        pos: String,
    },
    // typedef struct tm tm_t;
    StructAliasTypedef {
        pos: String,
        is_pub: bool,
        struct_name: String,
        type_alias: String,
    },
    Typedef(Typedef),
    StructTypedef(StructTypedef),
    ModuleVariable {
        type_name: Typename,
        form: Form,
        value: Expression,
        pos: String,
    },
    FunctionDeclaration(FunctionDeclaration),
}

#[derive(Debug, Clone)]
pub struct ImportNode {
    pub specified_path: String,
    pub pos: String,
}

#[derive(Debug, Clone)]
pub struct FunctionDeclaration {
    pub is_pub: bool,
    pub type_name: Typename,
    pub form: Form,
    pub parameters: FunctionParameters,
    pub body: Body,
    pub pos: String,
}

#[derive(Debug, Clone)]
pub struct Typedef {
    pub pos: String,
    pub is_pub: bool,
    pub alias: Identifier,
    pub type_name: Typename,
    pub dereference_count: usize,
    pub array_size: usize,
    pub function_parameters: Option<AnonymousParameters>,
}

#[derive(Debug, Clone)]
pub struct StructTypedef {
    pub pos: String,
    pub is_pub: bool,
    pub fields: Vec<StructEntry>,
    pub name: Identifier,
}

#[derive(Debug, Clone)]
pub struct FuncTypedef {
    pub is_pub: bool,
    pub return_type: Typename,
    pub name: String,
    pub params: AnonymousParameters,
}

#[derive(Debug, Clone)]
pub struct EnumItem {
    pub id: Identifier,
    pub value: Option<Expression>,
    pub pos: String,
}

#[derive(Debug, Clone)]
pub struct Typename {
    pub is_const: bool,
    pub name: NsName,
}

#[derive(Debug, Clone)]
pub struct NsName {
    pub namespace: String,
    pub name: String,
    pub pos: String,
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

#[derive(Debug, Clone)]
pub enum Expression {
    ArrayIndex {
        array: Box<Expression>,
        index: Box<Expression>,
    },
    BinaryOp {
        op: String,
        a: Box<Expression>,
        b: Box<Expression>,
    },
    FieldAccess {
        op: String,
        target: Box<Expression>,
        field_name: Identifier,
    },
    Cast {
        type_name: AnonymousTypeform,
        operand: Box<Expression>,
    },
    CompositeLiteral(CompositeLiteral),
    FunctionCall {
        function: Box<Expression>,
        arguments: Vec<Expression>,
    },
    Identifier(Identifier),
    Literal(Literal),
    NsName(NsName),
    PostfixOperator {
        operator: String,
        operand: Box<Expression>,
    },
    PrefixOperator {
        operator: String,
        operand: Box<Expression>,
    },
    Sizeof {
        argument: Box<SizeofArgument>,
    },
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
    pub pos: String,
}

#[derive(Debug, Clone)]
pub enum Statement {
    VariableDeclaration {
        type_name: Typename,
        form: Form,
        value: Option<Expression>,
        pos: String,
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
    Panic {
        arguments: Vec<Expression>,
        pos: String,
    },
    Return {
        expression: Option<Expression>,
    },
    Switch {
        value: Expression,
        cases: Vec<SwitchCase>,
        default_case: Option<Body>,
    },
    Expression(Expression),
}

#[derive(Debug, Clone)]
pub struct Form {
    pub stars: String,
    pub name: String,
    pub indexes: Vec<Option<Expression>>,
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
    pub pos: String,
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
