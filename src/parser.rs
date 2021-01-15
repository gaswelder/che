pub fn is_op(token_type: String) -> bool {
    let ops = [
        "+", "-", "*", "/", "=", "|", "&", "~", "^", "<", ">", "?", ":", "%", "+=", "-=", "*=",
        "/=", "%=", "&=", "^=", "|=", "++", "--", "->", ".", ">>", "<<", "<=", ">=", "&&", "||",
        "==", "!=", "<<=", ">>=",
    ];
    for op in ops.iter() {
        if token_type.eq(op) {
            return true;
        }
    }
    return false;
}

pub fn operator_strength(op: String) -> usize {
    let map = [
        vec![","],
        vec![
            "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=",
        ],
        vec!["||"],
        vec!["&&"],
        vec!["|"],
        vec!["^"],
        vec!["&"],
        vec!["!=", "=="],
        vec![">", "<", ">=", "<="],
        vec!["<<", ">>"],
        vec!["+", "-"],
        vec!["*", "/", "%"],
        vec!["prefix"],
        vec!["->", "."],
    ];
    for (i, ops) in map.iter().enumerate() {
        if ops.contains(&op.as_str()) {
            return i + 1;
        }
    }
    panic!("unknown operator: '{}'", op);
}
