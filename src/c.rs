#[derive(Debug, Clone)]
pub struct CModule {
    pub elements: Vec<ModElem>,
    // Ambient libraries to link with, populated with #link hints.
    pub link: Vec<String>,
}

// Old-school macro, #<name> <value>.
// Name is typically "define".
#[derive(Debug, Clone)]
pub struct Macro {
    pub name: String,
    pub value: String,
}

// enum { A = 1, B, ... };
#[derive(Debug, Clone)]
pub struct EnumDef {
    pub is_hidden: bool,
    pub members: Vec<CEnumItem>,
}

#[derive(Debug, Clone)]
pub struct CEnumItem {
    pub id: String,
    pub value: Option<CExpression>,
}

// typedef const char *foo_t;
#[derive(Debug, Clone)]
pub struct Typedef {
    pub is_pub: bool,
    pub type_name: Typename,
    pub form: CTypedefForm,
}

// int foo = 1
#[derive(Debug, Clone)]
pub struct VarDecl {
    pub type_name: Typename,
    pub form: CForm,
    pub value: CExpression,
}
// struct foo { int x; double y; }
#[derive(Debug, Clone)]
pub struct StructDef {
    pub name: String,
    pub fields: Vec<CStructItem>,
    pub is_pub: bool,
}

// const char *f(int, double);
#[derive(Debug, Clone)]
pub struct ForwardFunc {
    pub is_static: bool,
    pub type_name: Typename,
    pub form: CForm,
    pub parameters: CompatFunctionParameters,
}

// const char *f(int n, double x) { ... }
#[derive(Debug, Clone)]
pub struct FunctionDef {
    pub is_static: bool,
    pub type_name: Typename,
    pub form: CForm,
    pub parameters: CompatFunctionParameters,
    pub body: CBody,
}

#[derive(Debug, Clone)]
pub enum ModElem {
    Include(String),
    Macro(Macro),
    ForwardStruct(String),
    ForwardFunc(ForwardFunc),
    DefEnum(EnumDef),
    DefType(Typedef),
    DefStruct(StructDef),
    DeclVar(VarDecl),
    DefFunc(FunctionDef),
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

// <a> <op> <b>
#[derive(Debug, Clone)]
pub struct BinaryOp {
    pub op: String,
    pub a: Box<CExpression>,
    pub b: Box<CExpression>,
}

#[derive(Debug, Clone)]
pub enum CExpression {
    Literal(CLiteral),
    CompositeLiteral(CCompositeLiteral),
    Identifier(String),
    BinaryOp(BinaryOp),
    FieldAccess {
        op: String,
        target: Box<CExpression>,
        field_name: String,
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
    LoopCounterDeclaration(VarDecl),
}

#[derive(Debug, Clone)]
pub struct CForm {
    pub stars: String,
    pub name: String,
    pub indexes: Vec<Option<CExpression>>,
}

#[derive(Debug, Clone)]
pub enum CSizeofArgument {
    Typename(Typename),
    Expression(CExpression),
}

#[derive(Debug, Clone)]
pub enum CStatement {
    Block {
        statements: Vec<CStatement>,
    },
    Break,
    Continue,
    VariableDeclaration {
        type_name: Typename,
        forms: Vec<CForm>,
        values: Vec<Option<CExpression>>,
    },
    If {
        condition: CExpression,
        body: CBody,
        else_body: Option<CBody>,
    },
    For {
        init: Option<CForInit>,
        condition: Option<CExpression>,
        action: Option<CExpression>,
        body: CBody,
    },
    While {
        condition: CExpression,
        body: CBody,
    },
    Return {
        expression: Option<CExpression>,
    },
    Switch(Switch),
    Expression(CExpression),
}

// switch (x) { case 1: ... case 2: ... default: ... }
#[derive(Debug, Clone)]
pub struct Switch {
    pub value: CExpression,
    pub cases: Vec<CSwitchCase>,
    pub default: Option<CBody>,
}

#[derive(Debug, Clone)]
pub struct CSwitchCase {
    pub values: Vec<CSwitchCaseValue>,
    pub body: CBody,
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
    pub type_name: Typename,
    pub ops: Vec<String>,
}

#[derive(Debug, Clone)]
pub enum CSwitchCaseValue {
    Identifier(String),
    Literal(CLiteral),
}

#[derive(Debug, Clone)]
pub enum CLiteral {
    Char(String),
    String(String),
    Number(String),
    Null,
}

#[derive(Debug, Clone)]
pub struct Typename {
    pub is_const: bool,
    pub name: String,
}

#[derive(Debug, Clone)]
pub struct CTypeForm {
    pub type_name: Typename,
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
    pub type_name: Typename,
    pub form: CForm,
}
