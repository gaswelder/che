#[derive(Debug, Clone)]
pub struct CTypename {
    pub is_const: bool,
    pub name: String,
}

#[derive(Debug, Clone)]
pub struct CompatMacro {
    pub name: String,
    pub value: String,
}

#[derive(Debug, Clone)]
pub struct CAnonymousTypeform {
    pub type_name: CTypename,
    pub ops: Vec<String>,
}
