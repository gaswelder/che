use crate::{buf::Pos, nodes};

#[derive(Clone, Debug)]
pub struct Type {
    // The base type, for example "int" or "foo.bar_t".
    // This is not necessarily a native type, it could be an alias.
    pub base: nodes::NsName,

    // What operations have to be done to get to the base type.
    // [index, deref] means this is an array of pointers of the base type.
    pub ops: Vec<TypeOp>,
}

#[derive(Clone, Debug)]
pub enum TypeOp {
    Deref,
    Index,
    Call(Vec<Type>),
}

pub fn typeof_arith(a: &Type, b: &Type) -> Result<Type, String> {
    match (classify(a), classify(b)) {
        // T with T gives T
        (Class::CONSTNUM, Class::CONSTNUM) => Ok(a.clone()),
        (Class::FLT, Class::FLT) => Ok(widest_type(a, b)),
        (Class::SINT, Class::SINT) => Ok(widest_type(a, b)),
        (Class::UINT, Class::UINT) => Ok(widest_type(a, b)),

        // const with T gives T
        (Class::CONSTNUM, Class::FLT | Class::SINT | Class::UINT) => Ok(b.clone()),
        (Class::FLT | Class::SINT | Class::UINT, Class::CONSTNUM) => Ok(b.clone()),

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
        // T < T
        (Class::FLT, Class::FLT) => Ok(just("bool")),
        (Class::SINT, Class::SINT) => Ok(just("bool")),
        (Class::UINT, Class::UINT) => Ok(just("bool")),

        // num < const
        (Class::FLT | Class::SINT | Class::UINT, Class::CONSTNUM) => Ok(just("bool")),
        (Class::CONSTNUM, Class::FLT | Class::SINT | Class::UINT) => Ok(just("bool")),

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
    if is_booly(a) && is_booly(b) {
        Ok(just("bool"))
    } else {
        Err(format!("boolean comparison of {} and {}", a.fmt(), b.fmt()))
    }
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

enum Class {
    CONSTNUM,
    PTR,
    SINT,
    UINT,
    ARR,
    UNK,
    FLT,
    BOOL,
    NONE,
}

fn classify(x: &Type) -> Class {
    match () {
        _ if isconstnum(x) => Class::CONSTNUM,
        _ if is_pointer(x) => Class::PTR,
        _ if is_sint(x) => Class::SINT,
        _ if is_uint(x) => Class::UINT,
        _ if is_arr(x) => Class::ARR,
        _ if is_todo(x) => Class::UNK,
        _ if isfloat(x) => Class::FLT,
        _ if x.fmt() == "bool" => Class::BOOL,
        _ => Class::NONE,
    }
}

fn is_sint(x: &Type) -> bool {
    matches!(
        x.fmt().as_str(),
        "int" | "int16_t" | "int32_t" | "int64_t" | "int8_t"
		// sloppy
		| "char" | "ptrdiff_t"
    )
}

fn is_uint(x: &Type) -> bool {
    matches!(x.fmt().as_str(), |"size_t"| "uint16_t"
        | "uint32_t"
        | "uint64_t"
        | "uint8_t")
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

// Checks if the given type is a number literal.
fn isconstnum(b: &Type) -> bool {
    b.fmt() == "number"
}

fn isfloat(x: &Type) -> bool {
    x.fmt() == "float" || x.fmt() == "double"
}

pub fn is_todo(x: &Type) -> bool {
    x.fmt() == "*** TODO *** "
}

fn is_pointer(x: &Type) -> bool {
    x.ops.len() > 0 && matches!(x.ops[0], TypeOp::Deref)
}

fn is_arr(x: &Type) -> bool {
    x.ops.len() > 0 && matches!(x.ops[0], TypeOp::Index)
}

pub fn number() -> Type {
    just("number")
}

pub fn complit() -> Type {
    just("::complit")
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

pub fn addr(t: Type) -> Type {
    let mut ops = t.ops;
    ops.push(TypeOp::Deref);
    Type {
        ops,
        base: nsname(&t.base.ns, &t.base.name),
    }
}

pub fn deref(t: &Type) -> Result<Type, String> {
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

fn is_booly(a: &Type) -> bool {
    match classify(a) {
        Class::CONSTNUM => true,
        Class::PTR => true,
        Class::SINT => true,
        Class::UINT => true,
        Class::ARR => false,
        Class::UNK => true,
        Class::FLT => false,
        Class::NONE => false,
        Class::BOOL => true,
    }
}
