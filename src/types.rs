use crate::{buf::Pos, nodes};

#[derive(Clone, Debug)]
pub enum TypeOp {
    Deref,
    Index,
    Call(Vec<Type>),
}

#[derive(Clone, Debug)]
pub struct Type {
    // The base type, for example "int" or "foo.bar_t".
    // This is not necessarily a native type, it could be an alias.
    pub base: nodes::NsName,

    // What operations have to be done to get to the base type.
    // [index, deref] means this is an array of pointers of the base type.
    pub ops: Vec<TypeOp>,
}

impl Type {
    pub fn fmt(&self) -> String {
        let mut s = String::new();
        for o in &self.ops {
            match o {
                TypeOp::Deref => s.push_str("pointer to "),
                TypeOp::Index => s.push_str("array of "),
                TypeOp::Call(args) => {
                    let argss = args
                        .iter()
                        .map(|x| x.fmt())
                        .collect::<Vec<String>>()
                        .join(", ");
                    s.push_str(&format!("func ({}) to ", argss))
                }
            }
        }
        if self.base.ns != "" {
            s.push_str(&format!("{}.{}", self.base.ns, self.base.name));
        } else {
            s.push_str(&self.base.name);
        }
        s
    }
}

enum Class {
    CONSTNUM,
    PTR,
    SINT,
    UINT,
    CHAR,
    ARR,
    UNK,
    FLT,
    BOOL,
    NONE,
}

fn classify(x: &Type) -> Class {
    if x.ops.len() > 0 && matches!(x.ops[0], TypeOp::Deref) {
        return Class::PTR;
    }
    if x.ops.len() > 0 && matches!(x.ops[0], TypeOp::Index) {
        return Class::ARR;
    }
    if matches!(
        x.fmt().as_str(),
        "int" | "int16_t" | "int32_t" | "int64_t" | "int8_t"
		// sloppy
		| "ptrdiff_t"
    ) {
        return Class::SINT;
    }
    if matches!(x.fmt().as_str(), |"size_t"| "uint16_t"
        | "uint32_t"
        | "uint64_t"
        | "uint8_t")
    {
        return Class::UINT;
    }
    if x.fmt() == "float" || x.fmt() == "double" {
        return Class::FLT;
    }
    if x.fmt() == "number" {
        return Class::CONSTNUM;
    }
    if x.fmt() == "char" {
        return Class::CHAR;
    }
    if is_todo(x) {
        return Class::UNK;
    }
    if x.fmt() == "bool" {
        return Class::BOOL;
    }
    Class::NONE
}

fn typesize(x: &Type) -> usize {
    match x.fmt().as_str() {
        "size_t" => 64,
        "uint64_t" => 64,
        "int64_t" => 64,
        "uint32_t" => 32,
        "int32_t" => 32,
        "uint16_t" => 16,
        "int16_t" => 16,
        "uint8_t" => 8,
        "int8_t" => 8,
        "int" => 32,
        "char" => 8,
        "double" => 64,
        "float" => 32,
        // sloppy
        "time_t" => 32,
        "ptrdiff_t" => 64,
        _ => todo!("typesize {}", x.fmt()),
    }
}

fn widest_type(x: &Type, y: &Type) -> Type {
    if typesize(x) > typesize(y) {
        x.clone()
    } else {
        y.clone()
    }
}

fn is_booly(a: &Type) -> bool {
    match classify(a) {
        Class::PTR | Class::SINT | Class::UINT | Class::BOOL => true,
        Class::UNK => true,
        _ => false,
    }
}

pub fn is_ellipsis(t: &Type) -> bool {
    t.fmt() == "..."
}

pub fn is_todo(x: &Type) -> bool {
    x.fmt() == "*** TODO *** "
}

pub fn typeof_addr(t: Type) -> Type {
    let mut ops = t.ops;
    ops.push(TypeOp::Deref);
    Type {
        ops,
        base: nsname(&t.base.ns, &t.base.name),
    }
}

pub fn typeof_deref(t: &Type) -> Result<Type, String> {
    if is_todo(t) {
        return Ok(t.clone());
    }
    if !matches!(t.ops.first(), Some(TypeOp::Deref)) {
        return Err(format!("dereference of a non-pointer ({})", t.fmt()));
    }
    Ok(Type {
        ops: t.ops[1..].to_vec(),
        base: t.base.clone(),
    })
}

pub fn typeof_arith(a: &Type, b: &Type) -> Result<Type, String> {
    match (classify(a), classify(b)) {
        // T with T gives T
        (Class::CONSTNUM, Class::CONSTNUM) => Ok(a.clone()),
        (Class::FLT, Class::FLT) => Ok(widest_type(a, b)),
        (Class::SINT, Class::SINT) => Ok(widest_type(a, b)),
        (Class::UINT, Class::UINT) => Ok(widest_type(a, b)),

        // char with const gives int (sloppy)
        (Class::CHAR, Class::CONSTNUM) => Ok(just("int")),
        (Class::CONSTNUM, Class::CHAR) => Ok(just("int")),

        // const with T gives T
        (Class::CONSTNUM, Class::FLT | Class::SINT | Class::UINT) => Ok(b.clone()),
        (Class::FLT | Class::SINT | Class::UINT, Class::CONSTNUM) => Ok(a.clone()),

        // todo with x gives todo
        (_, Class::UNK) => Ok(b.clone()),
        (Class::UNK, _) => Ok(a.clone()),

        // sloppy
        (Class::UINT, Class::SINT) => Ok(widest_type(a, b)),
        (Class::SINT, Class::UINT) => Ok(widest_type(a, b)),
        (Class::SINT | Class::UINT, Class::FLT) => Ok(b.clone()),
        (Class::FLT, Class::SINT | Class::UINT) => Ok(a.clone()),
        (_, _) => Err(format!("arith on {}, {}", a.fmt(), b.fmt())),
    }
}

pub fn typeof_plusminus(op: &str, a: &Type, b: &Type) -> Result<Type, String> {
    match (op, classify(a), classify(b)) {
        // T + T = T
        (_, Class::CONSTNUM, Class::CONSTNUM) => Ok(a.clone()),
        (_, Class::SINT, Class::SINT) => Ok(widest_type(a, b)),
        (_, Class::UINT, Class::UINT) => Ok(widest_type(a, b)),
        (_, Class::FLT, Class::FLT) => Ok(widest_type(a, b)),

        // const + T = T
        (_, Class::CONSTNUM, Class::SINT | Class::UINT | Class::FLT) => Ok(b.clone()),
        (_, Class::SINT | Class::UINT | Class::FLT, Class::CONSTNUM) => Ok(b.clone()),

        // todo + T = todo
        (_, Class::UNK, _) => Ok(a.clone()),
        (_, _, Class::UNK) => Ok(b.clone()),

        // ptr - ptr = ptrdiff
        ("-", Class::PTR, Class::PTR) => Ok(just("ptrdiff_t")),

        // ptr + any int = ptr
        (_, Class::PTR | Class::ARR, Class::CONSTNUM | Class::SINT | Class::UINT) => Ok(a.clone()),
        (_, Class::CONSTNUM | Class::SINT | Class::UINT, Class::PTR | Class::ARR) => Ok(a.clone()),

        _ => Err(format!(
            "'{}' operation between {} and {}",
            op,
            a.fmt(),
            b.fmt()
        )),
    }
}

// Returns the type of numeric comparison between types a and b.
pub fn typeof_cmp(a: &Type, b: &Type) -> Result<Type, String> {
    match (classify(a), classify(b)) {
        // T == T
        (Class::FLT, Class::FLT) => Ok(just("bool")),
        (Class::SINT, Class::SINT) => Ok(just("bool")),
        (Class::UINT, Class::UINT) => Ok(just("bool")),
        (Class::CHAR, Class::CHAR) => Ok(just("bool")),

        // num == const
        (Class::FLT | Class::SINT | Class::UINT, Class::CONSTNUM) => Ok(just("bool")),
        (Class::CONSTNUM, Class::FLT | Class::SINT | Class::UINT) => Ok(just("bool")),

        // char == const
        (Class::CHAR, Class::CONSTNUM) => Ok(just("bool")),
        (Class::CONSTNUM, Class::CHAR) => Ok(just("bool")),

        // ints with chars
        (Class::CHAR, Class::SINT) => Ok(just("bool")),
        (Class::SINT, Class::CHAR) => Ok(just("bool")),

        // todo < T
        (Class::UNK, _) => Ok(just("bool")),
        (_, Class::UNK) => Ok(just("bool")),

        // ptr < ptr
        (Class::PTR, Class::PTR) => Ok(just("bool")),

        // hmm
        (Class::CONSTNUM, Class::CONSTNUM) => Ok(just("bool")),

        _ => Err(format!("comparison between {} and {}", a.fmt(), b.fmt())),
    }
}

pub fn typeof_boolcomp(a: &Type, b: &Type) -> Result<Type, String> {
    if !is_booly(a) {
        return Err(format!("{} used as boolean", a.fmt()));
    }
    if !is_booly(b) {
        return Err(format!("{} used as boolean", b.fmt()));
    }
    Ok(just("bool"))
}

pub fn typeof_index(arr: &Type, ind: &Type) -> Result<Type, String> {
    if is_todo(arr) {
        return Ok(arr.clone());
    }
    // sloppy
    if arr.ops.len() == 0 {
        return Ok(todo());
    }
    match arr.ops[0] {
        TypeOp::Index => {
            return Ok(Type {
                base: arr.base.clone(),
                ops: arr.ops[1..].to_vec(),
            });
        }
        TypeOp::Deref => {
            return Ok(Type {
                base: arr.base.clone(),
                ops: arr.ops[1..].to_vec(),
            });
        }
        _ => return Err(format!("index: ({})[{}]", arr.fmt(), ind.fmt())),
    };
}

//
// constructor wrappers
//

pub fn ellipsis() -> Type {
    just("...")
}

pub fn number() -> Type {
    just("number")
}

pub fn complit() -> Type {
    just("::complit")
}

pub fn just(name: &str) -> Type {
    Type {
        ops: Vec::new(),
        base: nsname("", name),
    }
}

pub fn justp(name: &str) -> Type {
    Type {
        ops: vec![TypeOp::Deref],
        base: nsname("", name),
    }
}

pub fn todo() -> Type {
    just("*** TODO *** ")
}

pub fn mk(ops: Vec<TypeOp>, ns: &str, name: &str) -> Type {
    Type {
        ops,
        base: nsname(ns, name),
    }
}

fn nsname(ns: &str, name: &str) -> nodes::NsName {
    nodes::NsName {
        ns: String::from(ns),
        name: String::from(name),
        pos: Pos { col: 0, line: 0 },
    }
}
