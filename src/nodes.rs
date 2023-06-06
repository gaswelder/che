#[derive(Clone)]
pub struct Module {
    pub id: String,
    pub elements: Vec<ModuleObject>,
    pub source_path: String,
}

#[derive(Debug, Clone)]
pub enum ModuleObject {
    Macro {
        name: String,
        value: String,
    },
    Import {
        path: String,
    },
    Enum {
        is_pub: bool,
        members: Vec<EnumItem>,
    },
    // typedef struct tm tm_t;
    StructAliasTypedef {
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
    },
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

#[derive(Debug, Clone)]
pub struct Typedef {
    pub is_pub: bool,
    pub type_name: Typename,
    pub form: TypedefForm,
}

#[derive(Debug, Clone)]
pub struct TypedefForm {
    pub dereference_count: usize,
    pub function_parameters: Option<AnonymousParameters>,
    pub array_size: usize,
    pub alias: String,
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
pub struct EnumItem {
    pub id: String,
    pub value: Option<Expression>,
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
pub enum SizeofArgument {
    Typename(Typename),
    Expression(Expression),
}

#[derive(Debug, Clone)]
pub struct Body {
    pub statements: Vec<Statement>,
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
    Identifier(String),
    Literal(Literal),
}

#[derive(Debug, Clone)]
pub struct Literal {
    pub type_name: String,
    pub value: String,
}

#[derive(Debug, Clone)]
pub struct SwitchCase {
    pub value: SwitchCaseValue,
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
