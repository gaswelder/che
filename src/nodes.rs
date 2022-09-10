#[derive(Clone)]
pub struct Module {
    pub id: String,
    pub elements: Vec<ModuleObject>,
}

#[derive(Debug, Clone)]
pub enum ModuleObject {
    CompatMacro(CompatMacro),
    Import { path: String },
    Enum(Enum),
    Typedef(Typedef),
    StructTypedef(StructTypedef),
    ModuleVariable(ModuleVariable),
    FunctionDeclaration(FunctionDeclaration),
}

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
pub struct FunctionDeclaration {
    pub is_pub: bool,
    pub type_name: Typename,
    pub form: Form,
    pub parameters: FunctionParameters,
    pub body: Body,
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
pub struct Literal {
    pub type_name: String,
    pub value: String,
}

#[derive(Debug, Clone)]
pub struct ArrayLiteral {
    pub values: Vec<ArrayLiteralEntry>,
}

#[derive(Debug, Clone)]
pub struct ArrayLiteralEntry {
    pub index: ArrayLiteralKey,
    pub value: ArrayLiteralValue,
}

#[derive(Debug, Clone)]
pub enum ArrayLiteralKey {
    None,
    Identifier(String),
    Literal(Literal),
}

#[derive(Debug, Clone)]
pub enum ArrayLiteralValue {
    ArrayLiteral(ArrayLiteral),
    Identifier(String),
    Literal(Literal),
}

#[derive(Debug, Clone)]
pub enum Expression {
    Literal(Literal),
    StructLiteral {
        members: Vec<StructLiteralMember>,
    },
    ArrayLiteral(Box<ArrayLiteral>),
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
pub struct CompatFunctionParameters {
    pub list: Vec<CompatFunctionParameter>,
    pub variadic: bool,
}

#[derive(Debug, Clone)]
pub struct TypeAndForms {
    pub type_name: Typename,
    pub forms: Vec<Form>,
}

#[derive(Debug, Clone)]
pub struct CompatFunctionParameter {
    pub type_name: Typename,
    pub form: Form,
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
pub struct ModuleVariable {
    pub type_name: Typename,
    pub form: Form,
    pub value: Expression,
}

#[derive(Debug, Clone)]
pub struct CompatFunctionForwardDeclaration {
    pub is_static: bool,
    pub type_name: Typename,
    pub form: Form,
    pub parameters: CompatFunctionParameters,
}

#[derive(Debug, Clone)]
pub struct CompatFunctionDeclaration {
    pub is_static: bool,
    pub type_name: Typename,
    pub form: Form,
    pub parameters: CompatFunctionParameters,
    pub body: Body,
}

#[derive(Debug, Clone)]
pub struct CompatStructDefinition {
    pub name: String,
    pub fields: Vec<CompatStructEntry>,
    pub is_pub: bool,
}

#[derive(Debug, Clone)]
pub enum CompatStructEntry {
    CompatStructField { type_name: Typename, form: Form },
    Union(Union),
}

#[derive(Debug, Clone)]
pub struct CompatMacro {
    pub name: String,
    pub value: String,
}

#[derive(Debug, Clone)]
pub struct TypedefForm {
    pub stars: String,
    pub params: Option<AnonymousParameters>,
    pub size: usize,
    pub alias: String,
}

#[derive(Debug, Clone)]
pub struct AnonymousParameters {
    pub forms: Vec<AnonymousTypeform>,
}

#[derive(Debug, Clone)]
pub enum StructEntry {
    Plain(TypeAndForms),
    Union(Union),
}

#[derive(Debug, Clone)]
pub struct CompatModule {
    pub elements: Vec<CompatModuleObject>,
    pub link: Vec<String>,
    pub id: String,
}

#[derive(Debug, Clone)]
pub enum CompatModuleObject {
    ModuleVariable(ModuleVariable),
    Typedef {
        is_pub: bool,
        type_name: Typename,
        form: TypedefForm,
    },
    CompatMacro(CompatMacro),
    CompatInclude(String),
    Enum {
        members: Vec<EnumItem>,
        is_hidden: bool,
    },
    CompatStructForwardDeclaration(String),
    CompatStructDefinition(CompatStructDefinition),
    CompatFunctionForwardDeclaration(CompatFunctionForwardDeclaration),
    CompatFunctionDeclaration(CompatFunctionDeclaration),
    CompatSplit {
        text: String,
    },
}
