use serde::{Deserialize, Serialize, Serializer};

#[derive(Serialize, Deserialize, Debug)]
pub struct Identifier {
    pub kind: String,
    pub name: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Type {
    pub kind: String,
    pub is_const: bool,
    pub type_name: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct AnonymousTypeform {
    pub kind: String,
    pub type_name: Type,
    pub ops: Vec<String>,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct AnonymousParameters {
    pub kind: String,
    pub forms: Vec<AnonymousTypeform>,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Literal {
    pub kind: String,
    pub type_name: String,
    pub value: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Enum {
    pub kind: String,
    pub is_pub: bool,
    pub members: Vec<EnumMember>,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct EnumMember {
    pub kind: String,
    pub id: Identifier,
    pub value: Option<Literal>,
}

#[derive(Debug, Serialize)]
pub struct ArrayLiteral {
    pub kind: String,
    pub values: Vec<ArrayLiteralEntry>,
}

#[derive(Debug)]
pub enum ArrayLiteralKey {
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
pub enum ArrayLiteralValue {
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
    pub index: ArrayLiteralKey,
    pub value: ArrayLiteralValue,
}

#[derive(Debug, Serialize)]
pub struct BinaryOp {
    pub kind: String,
    pub op: String,
    pub a: Expression,
    pub b: Expression,
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

#[derive(Debug, Serialize)]
pub struct Cast {
    pub kind: String,
    pub type_name: AnonymousTypeform,
    pub operand: Expression,
}

#[derive(Debug, Serialize)]
pub struct PostfixOperator {
    pub kind: String,
    pub operator: String,
    pub operand: Expression,
}

#[derive(Debug, Serialize)]
pub struct PrefixOperator {
    pub kind: String,
    pub operator: String,
    pub operand: Expression,
}

#[derive(Debug, Serialize)]
pub struct ArrayIndex {
    pub kind: String,
    pub array: Expression,
    pub index: Expression,
}

#[derive(Debug, Serialize)]
pub struct StructLiteralMember {
    pub name: Identifier,
    pub value: Expression,
}

#[derive(Debug, Serialize)]
pub struct StructLiteral {
    pub kind: String,
    pub members: Vec<StructLiteralMember>,
}

#[derive(Debug)]
pub enum SizeofArgument {
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
    pub kind: String,
    pub argument: SizeofArgument,
}

#[derive(Debug, Serialize)]
pub struct FunctionCall {
    pub kind: String,
    pub function: Expression,
    pub arguments: Vec<Expression>,
}

#[derive(Serialize, Debug)]
pub struct While {
    pub kind: String,
    pub condition: Expression,
    pub body: Body,
}

#[derive(Serialize, Debug)]
pub struct Body {
    pub kind: String,
    pub statements: Vec<Statement>,
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

#[derive(Serialize, Debug)]
pub struct VariableDeclaration {
    pub kind: String,
    pub type_name: Type,
    pub forms: Vec<Form>,
    pub values: Vec<Expression>,
}

#[derive(Serialize, Debug)]
pub struct Return {
    pub kind: String,
    pub expression: Option<Expression>,
}

#[derive(Serialize, Debug)]
pub struct Form {
    pub kind: String,
    pub stars: String,
    pub name: String,
    pub indexes: Vec<Option<Expression>>,
}

#[derive(Serialize, Debug)]
pub struct If {
    pub kind: String,
    pub condition: Expression,
    pub body: Body,
    pub else_body: Option<Body>,
}

#[derive(Serialize, Debug)]
pub struct For {
    pub kind: String,
    pub init: ForInit,
    pub condition: Expression,
    pub action: Expression,
    pub body: Body,
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
    pub kind: String,
    pub type_name: Type,
    pub name: Identifier,
    pub value: Expression,
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
    pub value: SwitchCaseValue,
    pub statements: Vec<Statement>,
}

#[derive(Serialize, Debug)]
pub struct Switch {
    pub kind: String,
    pub value: Expression,
    pub cases: Vec<SwitchCase>,
    pub default: Option<Vec<Statement>>,
}

#[derive(Debug, Serialize)]
pub struct FunctionParameters {
    pub list: Vec<FunctionParameter>,
    pub variadic: bool,
}

#[derive(Debug, Serialize)]
pub struct FunctionDeclaration {
    pub kind: String,
    pub is_pub: bool,
    pub type_name: Type,
    pub form: Form,
    pub parameters: FunctionParameters,
    pub body: Body,
}

#[derive(Serialize, Debug)]
pub struct FunctionParameter {
    pub type_name: Type,
    pub forms: Vec<Form>,
}

#[derive(Serialize, Debug)]
pub struct Union {
    pub kind: String,
    pub form: Form,
    pub fields: Vec<UnionField>,
}

#[derive(Serialize, Debug)]
pub struct UnionField {
    pub type_name: Type,
    pub form: Form,
}

#[derive(Serialize, Debug)]
pub struct ModuleVariable {
    pub kind: String,
    pub type_name: Type,
    pub form: Form,
    pub value: Expression,
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

#[derive(Serialize, Deserialize, Debug)]
pub struct Import {
    pub kind: String,
    pub path: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct CompatMacro {
    pub kind: String,
    pub name: String,
    pub value: String,
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
    pub kind: String,
    pub type_name: TypedefTarget,
    pub form: TypedefForm,
}

#[derive(Serialize, Debug)]
pub struct TypedefForm {
    pub stars: String,
    pub params: Option<AnonymousParameters>,
    pub size: usize,
    pub alias: Identifier,
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
    pub kind: String,
    pub fieldlists: Vec<StructEntry>,
}

#[derive(Serialize, Debug)]
pub struct StructFieldlist {
    pub kind: String,
    pub type_name: Type,
    pub forms: Vec<Form>,
}

#[derive(Serialize)]
pub struct Module {
    pub kind: String,
    pub elements: Vec<ModuleObject>,
}
