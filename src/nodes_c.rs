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
