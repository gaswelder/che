// A single C file/module.
#[derive(Debug, Clone)]
pub struct Module {
    pub elements: Vec<ModElem>, // code elements
    pub link: Vec<String>,      // OS libraries to link with, from #link hints.
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
    pub entries: Vec<EnumEntry>,
}

#[derive(Debug, Clone)]
pub struct EnumEntry {
    pub id: String,
    pub value: Option<Expr>,
}

// typedef const char *foo_t;
#[derive(Debug, Clone)]
pub struct Typedef {
    pub ispub: bool,
    pub typename: Typename,
    pub form: TypedefForm,
}

// int foo = 1
#[derive(Debug, Clone)]
pub struct VarDecl {
    pub typename: Typename,
    pub form: Form,
    pub value: Expr,
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
    pub form: Form,
    pub parameters: FuncParams,
}

// const char *f(int n, double x) { ... }
#[derive(Debug, Clone)]
pub struct FuncDef {
    pub is_static: bool,
    pub type_name: Typename,
    pub form: Form,
    pub parameters: FuncParams,
    pub body: Body,
}

#[derive(Debug, Clone)]
pub enum ModElem {
    Include(String),
    Macro(Macro),
    ForwardStruct(String),
    ForwardFunc(ForwardFunc),
    DefEnum(EnumDef),
    Typedef(Typedef),
    StuctDef(StructDef),
    VarDecl(VarDecl),
    FuncDef(FuncDef),
}

#[derive(Debug, Clone)]
pub struct FuncParams {
    pub list: Vec<CTypeForm>,
    pub variadic: bool,
}

#[derive(Debug, Clone)]
pub struct Body {
    pub statements: Vec<Statement>,
}

#[derive(Debug, Clone)]
pub struct CCompositeLiteral {
    pub entries: Vec<CCompositeLiteralEntry>,
}

#[derive(Debug, Clone)]
pub struct CCompositeLiteralEntry {
    pub is_index: bool,
    pub key: Option<Expr>,
    pub val: Expr,
}

// <a> <op> <b>
#[derive(Debug, Clone)]
pub struct BinaryOp {
    pub op: String,
    pub a: Box<Expr>,
    pub b: Box<Expr>,
}

#[derive(Debug, Clone)]
pub enum Expr {
    Literal(CLiteral),
    CompositeLiteral(CCompositeLiteral),
    Ident(String),
    BinaryOp(BinaryOp),
    FieldAccess {
        op: String,
        target: Box<Expr>,
        field_name: String,
    },
    PrefixOp {
        operator: String,
        operand: Box<Expr>,
    },
    PostfixOp {
        operator: String,
        operand: Box<Expr>,
    },
    Cast {
        type_name: BareTypeform,
        operand: Box<Expr>,
    },
    Call {
        func: Box<Expr>,
        args: Vec<Expr>,
    },
    Sizeof {
        arg: Box<SizeofArg>,
    },
    ArrayIndex {
        array: Box<Expr>,
        index: Box<Expr>,
    },
}

#[derive(Debug, Clone)]
pub enum ForInit {
    Expr(Expr),
    DeclLoopCounter(VarDecl),
}

#[derive(Debug, Clone)]
pub struct Form {
    pub stars: String,
    pub name: String,
    pub indexes: Vec<Option<Expr>>,
}

#[derive(Debug, Clone)]
pub enum SizeofArg {
    Typename(BareTypeform),
    Expression(Expr),
}

#[derive(Debug, Clone)]
pub enum Statement {
    Block {
        statements: Vec<Statement>,
    },
    Break,
    Continue,
    VarDecl {
        type_name: Typename,
        forms: Vec<Form>,
        values: Vec<Option<Expr>>,
    },
    If {
        condition: Expr,
        body: Body,
        else_body: Option<Body>,
    },
    For {
        init: Option<ForInit>,
        condition: Option<Expr>,
        action: Option<Expr>,
        body: Body,
    },
    While {
        cond: Expr,
        body: Body,
    },
    Return {
        expression: Option<Expr>,
    },
    Switch(Switch),
    Expression(Expr),
}

// switch (x) { case 1: ... case 2: ... default: ... }
#[derive(Debug, Clone)]
pub struct Switch {
    pub value: Expr,
    pub cases: Vec<CSwitchCase>,
    pub default: Option<Body>,
}

#[derive(Debug, Clone)]
pub struct CSwitchCase {
    pub values: Vec<CSwitchCaseValue>,
    pub body: Body,
}

#[derive(Debug, Clone)]
pub struct TypedefForm {
    pub stars: String,
    pub params: Option<CAnonymousParameters>,
    pub size: usize,
    pub alias: String,
}

#[derive(Debug, Clone)]
pub struct CAnonymousParameters {
    pub ellipsis: bool,
    pub forms: Vec<BareTypeform>,
}

#[derive(Debug, Clone)]
pub struct BareTypeform {
    pub typename: Typename,
    pub hops: usize,
}

#[derive(Debug, Clone)]
pub enum CSwitchCaseValue {
    Ident(String),
    Literal(CLiteral),
}

#[derive(Debug, Clone)]
pub enum CLiteral {
    Char(String),
    String(Vec<String>),
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
    pub form: Form,
}

#[derive(Debug, Clone)]
pub enum CStructItem {
    Field(CTypeForm),
    Union(CUnion),
}

#[derive(Debug, Clone)]
pub struct CUnion {
    pub form: Form,
    pub fields: Vec<CUnionField>,
}

#[derive(Debug, Clone)]
pub struct CUnionField {
    pub type_name: Typename,
    pub form: Form,
}

// Module synopsis is what you would extract into a header file:
// function prototypes, typedefs, struct declarations.
pub fn get_module_synopsis(module: &Module) -> Vec<ModElem> {
    let mut elements: Vec<ModElem> = vec![];

    for element in &module.elements {
        match element {
            ModElem::Typedef(x) => {
                if x.ispub {
                    elements.push(ModElem::Typedef(x.clone()))
                }
            }
            ModElem::StuctDef(x) => {
                if x.is_pub {
                    elements.push(ModElem::StuctDef(x.clone()))
                }
            }
            ModElem::ForwardFunc(x) => {
                if !x.is_static {
                    elements.push(ModElem::ForwardFunc(x.clone()))
                }
            }
            ModElem::Macro(x) => elements.push(ModElem::Macro(Macro {
                name: x.name.clone(),
                value: x.value.clone(),
            })),
            ModElem::DefEnum(x) => {
                if !x.is_hidden {
                    elements.push(ModElem::DefEnum(x.clone()))
                }
            }
            _ => {}
        }
    }
    return elements;
}
