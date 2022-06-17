#[derive(Clone)]
pub struct Module {
    pub id: String,
    pub elements: Vec<ModuleObject>,
}

#[derive(Debug, Clone)]
pub enum ModuleObject {
    ModuleVariable(ModuleVariable),
    Import {
        path: String,
    },
    CompatMacro(CompatMacro),
    Enum {
        is_pub: bool,
        members: Vec<EnumMember>,
    },
    FunctionDeclaration {
        is_pub: bool,
        type_name: Typename,
        form: Form,
        parameters: FunctionParameters,
        body: Body,
    },
    Typedef {
        is_pub: bool,
        type_name: TypedefTarget,
        form: TypedefForm,
    },
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
pub struct EnumMember {
    pub id: String,
    pub value: Option<Expression>,
}

#[derive(Debug, Clone)]
pub struct ArrayLiteral {
    pub values: Vec<ArrayLiteralEntry>,
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
pub struct ArrayLiteralEntry {
    pub index: ArrayLiteralKey,
    pub value: ArrayLiteralValue,
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
pub struct While {
    pub condition: Expression,
    pub body: Body,
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
    If(If),
    For(For),
    While(While),
    Return(Return),
    Switch(Switch),
    Expression(Expression),
}

#[derive(Debug, Clone)]
pub struct Return {
    pub expression: Option<Expression>,
}

#[derive(Debug, Clone)]
pub struct Form {
    pub stars: String,
    pub name: String,
    pub indexes: Vec<Option<Expression>>,
}

#[derive(Debug, Clone)]
pub struct If {
    pub condition: Expression,
    pub body: Body,
    pub else_body: Option<Body>,
}

#[derive(Debug, Clone)]
pub struct For {
    pub init: ForInit,
    pub condition: Expression,
    pub action: Expression,
    pub body: Body,
}

#[derive(Debug, Clone)]
pub enum ForInit {
    Expression(Expression),
    LoopCounterDeclaration(LoopCounterDeclaration),
}

#[derive(Debug, Clone)]
pub struct LoopCounterDeclaration {
    pub type_name: Typename,
    pub form: Form,
    pub value: Expression,
}

#[derive(Debug, Clone)]
pub enum SwitchCaseValue {
    Identifier(String),
    Literal(Literal),
}

#[derive(Debug, Clone)]
pub struct SwitchCase {
    pub value: SwitchCaseValue,
    pub statements: Vec<Statement>,
}

#[derive(Debug, Clone)]
pub struct Switch {
    pub value: Expression,
    pub cases: Vec<SwitchCase>,
    pub default: Option<Vec<Statement>>,
}

#[derive(Debug, Clone)]
pub struct FunctionParameters {
    pub list: Vec<FunctionParameter>,
    pub variadic: bool,
}

#[derive(Debug, Clone)]
pub struct CompatFunctionParameters {
    pub list: Vec<CompatFunctionParameter>,
    pub variadic: bool,
}

#[derive(Debug, Clone)]
pub struct FunctionParameter {
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
pub enum TypedefTarget {
    AnonymousStruct(AnonymousStruct),
    Typename(Typename),
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
    StructFieldlist(StructFieldlist),
    Union(Union),
}

#[derive(Debug, Clone)]
pub struct AnonymousStruct {
    pub entries: Vec<StructEntry>,
}

#[derive(Debug, Clone)]
pub struct StructFieldlist {
    pub type_name: Typename,
    pub forms: Vec<Form>,
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
        type_name: TypedefTarget,
        form: TypedefForm,
    },
    CompatMacro(CompatMacro),
    CompatInclude(String),
    Enum {
        members: Vec<EnumMember>,
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
