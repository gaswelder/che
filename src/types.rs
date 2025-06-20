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
    Call,
}

enum Class {
    CONSTNUM,
    PTR,
    SINT,
    UINT,
    ARR,
    UNK,
    FLT,
    NONE,
}

fn classify(x: &Type) -> Class {
    match () {
        _ if isconstnum(x) => Class::CONSTNUM,
        _ if is_pointer(x) => Class::PTR,
        _ if isint(x) => Class::SINT,
        _ if isuint(x) => Class::UINT,
        _ if is_arr(x) => Class::ARR,
        _ if is_unknown(x) => Class::UNK,
        _ if isfloat(x) => Class::FLT,
        _ => Class::NONE,
    }
}

// Checks if the given type is an integer type.
pub fn isint(x: &Type) -> bool {
    matches!(
        x.fmt().as_str(),
        "int" | "int16_t" | "int32_t" | "int64_t" | "int8_t"
		// sloppy
		| "char" | "ptrdiff_t" | "time_t"
    )
}

fn isuint(x: &Type) -> bool {
    matches!(x.fmt().as_str(), |"size_t"| "uint16_t"
        | "uint32_t"
        | "uint64_t"
        | "uint8_t")
}

fn isanyint(x: &Type) -> bool {
    isint(x) || isuint(x)
}

fn intsize(x: &Type) -> usize {
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
        _ => todo!("intsize {:?}", x),
    }
}

fn widest_integer(x: &Type, y: &Type) -> Type {
    if intsize(x) > intsize(y) {
        x.clone()
    } else {
        y.clone()
    }
}

// Checks if the given type is a number literal.
pub fn isconstnum(b: &Type) -> bool {
    b.fmt() == "number"
}

pub fn isfloat(x: &Type) -> bool {
    x.fmt() == "float" || x.fmt() == "double"
}

pub fn widest_float(a: &Type, b: &Type) -> Type {
    if a.fmt() == "double" {
        a.clone()
    } else {
        b.clone()
    }
}

// Checks if the given type is unknown.
pub fn is_unknown(x: &Type) -> bool {
    x.fmt() == "*** UNKNOWN *** "
}

pub fn is_pointer(x: &Type) -> bool {
    x.ops.len() > 0 && matches!(x.ops[0], TypeOp::Deref)
}

fn is_arr(x: &Type) -> bool {
    x.ops.len() > 0 && matches!(x.ops[0], TypeOp::Index)
}

pub fn constcharp() -> Type {
    Type {
        ops: vec![TypeOp::Deref],
        base: nsname("", "char"),
    }
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

pub fn unk() -> Type {
    just("*** UNKNOWN *** ")
}

impl Type {
    pub fn fmt(&self) -> String {
        let mut s = String::new();
        for o in &self.ops {
            match o {
                TypeOp::Deref => s.push_str("pointer to "),
                TypeOp::Index => s.push_str("array of "),
                TypeOp::Call => s.push_str("func to "),
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
    if is_unknown(t) {
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

// Returns the type of numeric comparison between types a and b.
pub fn typeof_cmp(a: &Type, b: &Type) -> Result<Type, String> {
    if (a.fmt() == b.fmt())
        || (isanyint(a) && isconstnum(b))
        || (isint(a) && isint(b))
        || (isuint(a) && isuint(b))
        || (isfloat(a) && isconstnum(b))
        || is_unknown(a)
        || is_unknown(b)
        || (is_pointer(a) && is_pointer(b))
    {
        Ok(just("bool"))
    } else {
        Err(format!("comparison between {} and {}", a.fmt(), b.fmt()))
    }
}

pub fn typeof_plusminus(op: &str, a: &Type, b: &Type) -> Result<Type, String> {
    match (op, classify(a), classify(b)) {
        (_, Class::CONSTNUM, Class::CONSTNUM) => Ok(a.clone()),
        (_, Class::CONSTNUM, Class::SINT | Class::UINT | Class::FLT) => Ok(b.clone()),
        (_, Class::PTR | Class::ARR, Class::CONSTNUM | Class::SINT | Class::UINT) => Ok(a.clone()),
        ("-", Class::PTR, Class::PTR) => Ok(just("ptrdiff_t")),
        (_, Class::SINT | Class::UINT | Class::FLT, Class::CONSTNUM) => Ok(a.clone()),
        (_, Class::SINT, Class::SINT) => Ok(a.clone()),
        (_, Class::UINT, Class::UINT) => Ok(a.clone()),
        (_, Class::FLT, Class::FLT) => Ok(a.clone()),
        (_, Class::UNK, _) => Ok(a.clone()),
        (_, _, Class::UNK) => Ok(b.clone()),
        _ => Err(format!(
            "'{}' operation between {} and {}",
            op,
            a.fmt(),
            b.fmt()
        )),
    }
}

pub fn typeof_boolcomp(a: &Type, b: &Type) -> Result<Type, String> {
    let af = a.fmt();
    let bf = b.fmt();
    let aunk = is_unknown(a);
    let bunk = is_unknown(b);
    let abool = af == "bool";
    let bbool = bf == "bool";
    if (abool && bbool)
        || (aunk && bbool)
        || (bunk && abool)
        || (aunk && bunk)
		// sloppy mode
		|| (is_pointer(a) && bbool)
		|| (is_pointer(b) && abool)
		|| (isanyint(a) && bunk)
		|| (abool && isanyint(b))
		|| (bbool && isanyint(a))
    {
        Ok(just("bool"))
    } else {
        Err(format!("boolean comparison of {} and {}", af, bf))
    }
}

pub fn typeof_index(arr: &Type, ind: &Type) -> Type {
    if is_unknown(arr) {
        return arr.clone();
    }
    if !isconstnum(&ind) && !isanyint(&ind) {
        return unk();
    }
    if arr.ops.len() == 0 {
        return unk();
    }
    match arr.ops[0] {
        TypeOp::Index => Type {
            base: arr.base.clone(),
            ops: arr.ops[1..].to_vec(),
        },
        TypeOp::Deref => Type {
            base: arr.base.clone(),
            ops: arr.ops[1..].to_vec(),
        },
        _ => {
            println!("index: ({})[{}]", arr.fmt(), ind.fmt());
            todo!()
        }
    }
}

fn isarr(a: &Type) -> bool {
    if a.ops.len() == 0 {
        return false;
    }
    match a.ops[0] {
        TypeOp::Deref | TypeOp::Index => true,
        TypeOp::Call => false,
    }
}

pub fn typeof_arith(a: &Type, b: &Type) -> Result<Type, String> {
    if a.fmt() == b.fmt() {
        return Ok(a.clone());
    }
    if isarr(a) && isconstnum(b) {
        return Ok(a.clone());
    }
    if isarr(a) && isanyint(b) {
        return Ok(a.clone());
    }
    if isanyint(a) && isconstnum(b) {
        return Ok(a.clone());
    }
    if isconstnum(a) && isanyint(b) {
        return Ok(b.clone());
    }
    if isanyint(a) && isanyint(b) {
        return Ok(widest_integer(a, b));
    }
    if isfloat(a) && isconstnum(b) {
        return Ok(a.clone());
    }
    if isfloat(b) && isconstnum(a) {
        return Ok(b.clone());
    }
    if isconstnum(a) && isfloat(b) {
        return Ok(b.clone());
    }
    // '0' + offset
    if a.fmt() == "char" && isanyint(b) {
        return Ok(a.clone());
    }
    if isanyint(a) && is_unknown(b) {
        return Ok(a.clone());
    }
    if isconstnum(a) && is_unknown(b) {
        return Ok(unk());
    }
    // sloppy mode
    if isfloat(a) && isfloat(b) {
        return Ok(widest_float(a, b));
    }
    if isfloat(a) && isanyint(b) {
        return Ok(a.clone());
    }
    if isanyint(a) && isfloat(b) {
        return Ok(b.clone());
    }
    if is_unknown(a) || is_unknown(b) {
        return Ok(unk());
    }
    Err(format!("arith on {}, {}", a.fmt(), b.fmt()))
}
