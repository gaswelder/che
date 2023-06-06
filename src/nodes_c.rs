#[derive(Debug, Clone)]
pub struct CEnumItem {
    pub id: String,
    pub value: Option<CExpression>,
}

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
    StructForwardDeclaration(String),
    StructDefinition {
        name: String,
        fields: Vec<CStructItem>,
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
    pub list: Vec<CTypeForm>,
    pub variadic: bool,
}

#[derive(Debug, Clone)]
pub struct CBody {
    pub statements: Vec<CStatement>,
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
pub enum CExpression {
    Literal(CLiteral),
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
pub enum CForInit {
    Expression(CExpression),
    LoopCounterDeclaration {
        type_name: CTypename,
        form: CForm,
        value: CExpression,
    },
}

#[derive(Debug, Clone)]
pub struct CForm {
    pub stars: String,
    pub name: String,
    pub indexes: Vec<Option<CExpression>>,
}

#[derive(Debug, Clone)]
pub enum CSizeofArgument {
    Typename(CTypename),
    Expression(CExpression),
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
pub struct CSwitchCase {
    pub value: CSwitchCaseValue,
    pub body: CBody,
}

#[derive(Debug, Clone)]
pub struct CompatMacro {
    pub name: String,
    pub value: String,
}

#[derive(Debug, Clone)]
pub struct CTypedefForm {
    pub stars: String,
    pub params: Option<CAnonymousParameters>,
    pub size: usize,
    pub alias: String,
}

#[derive(Debug, Clone)]
pub struct CAnonymousParameters {
    pub ellipsis: bool,
    pub forms: Vec<CAnonymousTypeform>,
}

#[derive(Debug, Clone)]
pub struct CAnonymousTypeform {
    pub type_name: CTypename,
    pub ops: Vec<String>,
}

#[derive(Debug, Clone)]
pub enum CSwitchCaseValue {
    Identifier(String),
    Literal(CLiteral),
}

#[derive(Debug, Clone)]
pub struct CLiteral {
    pub type_name: String,
    pub value: String,
}

#[derive(Debug, Clone)]
pub struct CTypename {
    pub is_const: bool,
    pub name: String,
}

#[derive(Debug, Clone)]
pub struct CTypeForm {
    pub type_name: CTypename,
    pub form: CForm,
}

#[derive(Debug, Clone)]
pub enum CStructItem {
    Field(CTypeForm),
    Union(CUnion),
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
