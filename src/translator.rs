use crate::build::Ctx;
use crate::c;
use crate::format_c;
use crate::format_che;
use crate::nodes;
use std::collections::HashSet;

// Generates a unique identifier prefix for the given module path.
pub fn module_gid(path: &String) -> String {
    return format!(
        "ns_{}",
        path.replace("/", "_").replace(".", "_").replace("-", "_")
    );
}

// Module synopsis is what you would extract into a header file:
// function prototypes, typedefs, struct declarations.
pub fn get_module_synopsis(module: &c::CModule) -> Vec<c::ModElem> {
    let mut elements: Vec<c::ModElem> = vec![];

    for element in &module.elements {
        match element {
            c::ModElem::DefType(x) => {
                if x.is_pub {
                    elements.push(c::ModElem::DefType(x.clone()))
                }
            }
            c::ModElem::DefStruct(x) => {
                if x.is_pub {
                    elements.push(c::ModElem::DefStruct(x.clone()))
                }
            }
            c::ModElem::ForwardFunc(x) => {
                if !x.is_static {
                    elements.push(c::ModElem::ForwardFunc(x.clone()))
                }
            }
            c::ModElem::Macro(x) => {
                // if name == "include" {
                elements.push(c::ModElem::Macro(c::Macro {
                    name: x.name.clone(),
                    value: x.value.clone(),
                }))
                // }
            }
            c::ModElem::DefEnum(x) => {
                if !x.is_hidden {
                    elements.push(c::ModElem::DefEnum(x.clone()))
                }
            }
            _ => {}
        }
    }
    return elements;
}

// Translates a module to c module.
pub fn translate(m: &nodes::Module, ctx: &Ctx) -> c::CModule {
    // Build the list of modules to link.
    // These are specified using the #link macros.
    let mut link: Vec<String> = Vec::new();
    for node in &m.elements {
        match node {
            nodes::ModElem::Macro(x) => {
                if x.name == "link" {
                    link.push(x.value.clone())
                }
            }
            _ => {}
        }
    }

    let mut elements: Vec<c::ModElem> = Vec::new();
    for element in &m.elements {
        for node in tr_mod_elem(element, ctx) {
            elements.push(node)
        }
    }

    // Reorder the elements so that typedefs and similar preamble elements
    // come first.
    let mut groups: Vec<Vec<c::ModElem>> =
        vec![vec![], vec![], vec![], vec![], vec![], vec![], vec![]];
    let mut set: HashSet<String> = HashSet::new();
    for element in elements {
        let order = match element {
            c::ModElem::Include(_) => 0,
            c::ModElem::Macro { .. } => 0,
            c::ModElem::ForwardStruct(_) => 1,
            c::ModElem::DefType { .. } => 2,
            c::ModElem::DefStruct { .. } => 3,
            c::ModElem::DefEnum { .. } => 3,
            c::ModElem::ForwardFunc { .. } => 4,
            c::ModElem::DeclVar { .. } => 5,
            _ => 6,
        };

        match &element {
            c::ModElem::DefType(x) => {
                let s = format_c::format_typedef(&x);
                if set.contains(&s) {
                    continue;
                }
                set.insert(s);
            }
            c::ModElem::DefStruct(x) => {
                let s = format_c::format_compat_struct_definition(&x);
                if set.contains(&s) {
                    continue;
                }
                set.insert(s);
            }
            _ => {}
        }
        groups[order].push(element);
    }
    let mut sorted_elements: Vec<c::ModElem> = vec![];
    for group in groups {
        for e in group {
            sorted_elements.push(e)
        }
    }
    return c::CModule {
        elements: sorted_elements,
        link,
    };
}

fn tr_mod_elem(element: &nodes::ModElem, ctx: &Ctx) -> Vec<c::ModElem> {
    match element {
        nodes::ModElem::Import(_) => Vec::new(),
        nodes::ModElem::Typedef(x) => tr_typedef(&x, ctx),
        nodes::ModElem::StructAliasTypedef(x) => tr_struct_alias(&x),
        nodes::ModElem::StructTypedef(x) => tr_struct_typedef(&x, ctx),
        nodes::ModElem::FunctionDeclaration(x) => tr_func_decl(&x, ctx),
        nodes::ModElem::Enum(x) => tr_enum(&x, ctx),
        nodes::ModElem::Macro(x) => tr_macro(&x),
        nodes::ModElem::ModuleVariable(x) => tr_mod_var(&x, ctx),
    }
}

fn tr_expr(e: &nodes::Expression, ctx: &Ctx) -> c::CExpression {
    return match e {
        nodes::Expression::NsName(n) => c::CExpression::Identifier(tr_ns_name(n, ctx)),
        nodes::Expression::ArrayIndex(x) => tr_array_index(&x, ctx),
        nodes::Expression::BinaryOp(x) => tr_binary_op(&x, ctx),
        nodes::Expression::FieldAccess(x) => tr_field_access(&x, ctx),
        nodes::Expression::CompositeLiteral(x) => tr_comp_literal(&x, ctx),
        nodes::Expression::Literal(x) => c::CExpression::Literal(tr_literal(x)),
        nodes::Expression::Identifier(x) => c::CExpression::Identifier(x.name.clone()),
        nodes::Expression::PrefixOperator(x) => tr_prefix(&x, ctx),
        nodes::Expression::PostfixOperator(x) => tr_postfix(&x, ctx),
        nodes::Expression::Cast(x) => tr_cast(&x, ctx),
        nodes::Expression::FunctionCall(x) => tr_call(&x, ctx),
        nodes::Expression::Sizeof(x) => tr_sizeof(&x, ctx),
    };
}

fn tr_body(b: &nodes::Body, ctx: &Ctx) -> c::CBody {
    let mut statements: Vec<c::CStatement> = Vec::new();
    for s in &b.statements {
        statements.push(match s {
            nodes::Statement::Break => c::CStatement::Break,
            nodes::Statement::Continue => c::CStatement::Continue,
            nodes::Statement::Expression(x) => c::CStatement::Expression(tr_expr(&x, ctx)),
            nodes::Statement::For(x) => tr_for(x, ctx),
            nodes::Statement::If(x) => tr_if(x, ctx),
            nodes::Statement::Return(x) => tr_return(x, ctx),
            nodes::Statement::Panic(x) => tr_panic(x, ctx),
            nodes::Statement::Switch(x) => tr_switch(x, ctx),
            nodes::Statement::VariableDeclaration(x) => tr_vardecl(x, ctx),
            nodes::Statement::While(x) => tr_while(x, ctx),
        });
    }
    return c::CBody { statements };
}

fn tr_struct_alias(x: &nodes::StructAlias) -> Vec<c::ModElem> {
    vec![c::ModElem::DefType(c::Typedef {
        is_pub: x.is_pub,
        type_name: c::Typename {
            is_const: false,
            name: format!("struct {}", x.struct_name),
        },
        form: c::CTypedefForm {
            stars: String::new(),
            params: None,
            size: 0,
            alias: x.type_alias.clone(),
        },
    })]
}

fn tr_typedef(x: &nodes::Typedef, ctx: &Ctx) -> Vec<c::ModElem> {
    let array_size = &x.array_size;
    let alias = &x.alias;
    let function_parameters = &x.function_parameters;
    let dereference_count = &x.dereference_count;
    let type_name = &x.type_name;
    vec![c::ModElem::DefType(c::Typedef {
        is_pub: x.is_pub,
        type_name: tr_typename(type_name, ctx),
        form: c::CTypedefForm {
            stars: "*".repeat(*dereference_count),
            params: function_parameters.as_ref().map(|x| {
                let mut forms: Vec<c::CAnonymousTypeform> = Vec::new();
                for f in &x.forms {
                    forms.push(translate_anonymous_typeform(&f, ctx));
                }
                c::CAnonymousParameters {
                    ellipsis: x.ellipsis,
                    forms,
                }
            }),
            size: *array_size,
            alias: alias.name.clone(),
        },
    })]
}

fn tr_struct_typedef(x: &nodes::StructTypedef, ctx: &Ctx) -> Vec<c::ModElem> {
    // A sugary "typedef {int a} foo_t" is translated to
    // "struct __foo_t_struct {int a}; typedef __foo_t_struct foo_t;".
    // And remember that the struct definition itself is sugar that
    // should be translated as well.

    let name = &x.name;
    let fields = &x.fields;
    let struct_name = format!("__{}_struct", name.name);

    // Build the compat struct fields.
    let mut compat_fields: Vec<c::CStructItem> = Vec::new();
    for entry in fields {
        match entry {
            // One fieldlist is multiple fields of the same type.
            nodes::StructEntry::Plain(x) => {
                for f in &x.forms {
                    compat_fields.push(c::CStructItem::Field(c::CTypeForm {
                        type_name: tr_typename(&x.type_name, ctx),
                        form: tr_form(f, ctx),
                    }));
                }
            }
            nodes::StructEntry::Union(x) => {
                compat_fields.push(c::CStructItem::Union(tr_union(x, ctx)));
            }
        }
    }
    vec![
        c::ModElem::ForwardStruct(struct_name.clone()),
        c::ModElem::DefStruct(c::StructDef {
            name: struct_name.clone(),
            fields: compat_fields,
            is_pub: x.is_pub,
        }),
        c::ModElem::DefType(c::Typedef {
            is_pub: x.is_pub,
            type_name: c::Typename {
                is_const: false,
                name: format!("struct {}", struct_name.clone()),
            },
            form: c::CTypedefForm {
                stars: "".to_string(),
                size: 0,
                params: None,
                alias: name.name.clone(),
            },
        }),
    ]
}

fn tr_enum(x: &nodes::Enum, ctx: &Ctx) -> Vec<c::ModElem> {
    let mut tm: Vec<c::CEnumItem> = Vec::new();
    for m in &x.members {
        tm.push(c::CEnumItem {
            id: m.id.name.clone(),
            value: m.value.as_ref().map(|v| tr_expr(&v, ctx)),
        })
    }
    return vec![c::ModElem::DefEnum(c::EnumDef {
        members: tm,
        is_hidden: !x.is_pub,
    })];
}

fn tr_macro(x: &nodes::Macro) -> Vec<c::ModElem> {
    let name = &x.name;
    if name == "type" || name == "link" {
        return vec![];
    }
    return vec![c::ModElem::Macro(c::Macro {
        name: name.clone(),
        value: x.value.clone(),
    })];
}

fn tr_mod_var(x: &nodes::VariableDeclaration, ctx: &Ctx) -> Vec<c::ModElem> {
    vec![c::ModElem::DeclVar(c::VarDecl {
        type_name: tr_typename(&x.type_name, ctx),
        form: tr_form(&x.form, ctx),
        value: tr_expr(x.value.as_ref().unwrap(), ctx),
    })]
}

fn tr_form(f: &nodes::Form, ctx: &Ctx) -> c::CForm {
    let mut indexes: Vec<Option<c::CExpression>> = vec![];
    for index in &f.indexes {
        indexes.push(match index {
            Some(e) => Some(tr_expr(&e, ctx)),
            None => None,
        })
    }
    return c::CForm {
        indexes,
        name: f.name.clone(),
        stars: "*".repeat(f.hops),
    };
}

fn tr_union(x: &nodes::Union, ctx: &Ctx) -> c::CUnion {
    let mut fields: Vec<c::CUnionField> = Vec::new();
    for f in &x.fields {
        fields.push(c::CUnionField {
            type_name: tr_typename(&f.type_name, ctx),
            form: tr_form(&f.form, ctx),
        })
    }
    return c::CUnion {
        form: tr_form(&x.form, ctx),
        fields,
    };
}

fn tr_binary_op(x: &nodes::BinaryOp, ctx: &Ctx) -> c::CExpression {
    let op = &x.op;
    let a = &x.a;
    let b = &x.b;
    c::CExpression::BinaryOp(c::BinaryOp {
        op: op.clone(),
        a: Box::new(tr_expr(a, ctx)),
        b: Box::new(tr_expr(b, ctx)),
    })
}

fn tr_field_access(x: &nodes::FieldAccess, ctx: &Ctx) -> c::CExpression {
    let op = &x.op;
    let target = &x.target;
    let field_name = &x.field_name;
    c::CExpression::FieldAccess {
        op: op.clone(),
        target: Box::new(tr_expr(target, ctx)),
        field_name: field_name.name.clone(),
    }
}

fn tr_comp_literal(x: &nodes::CompositeLiteral, ctx: &Ctx) -> c::CExpression {
    let mut entries: Vec<c::CCompositeLiteralEntry> = Vec::new();
    for e in &x.entries {
        entries.push(c::CCompositeLiteralEntry {
            is_index: e.is_index,
            key: e.key.as_ref().map(|x| tr_expr(&x, ctx)),
            value: tr_expr(&e.value, ctx),
        })
    }
    c::CExpression::CompositeLiteral(c::CCompositeLiteral { entries })
}

fn tr_prefix(x: &nodes::PrefixOperator, ctx: &Ctx) -> c::CExpression {
    let operator = &x.operator;
    let operand = &x.operand;
    c::CExpression::PrefixOperator {
        operator: operator.clone(),
        operand: Box::new(tr_expr(operand, ctx)),
    }
}

fn tr_postfix(x: &nodes::PostfixOperator, ctx: &Ctx) -> c::CExpression {
    let operator = &x.operator;
    let operand = &x.operand;
    c::CExpression::PostfixOperator {
        operator: operator.clone(),
        operand: Box::new(tr_expr(operand, ctx)),
    }
}

fn tr_cast(x: &nodes::Cast, ctx: &Ctx) -> c::CExpression {
    let type_name = &x.type_name;
    let operand = &x.operand;
    c::CExpression::Cast {
        type_name: translate_anonymous_typeform(type_name, ctx),
        operand: Box::new(tr_expr(operand, ctx)),
    }
}

fn tr_array_index(x: &nodes::ArrayIndex, ctx: &Ctx) -> c::CExpression {
    c::CExpression::ArrayIndex {
        array: Box::new(tr_expr(&x.array, ctx)),
        index: Box::new(tr_expr(&x.index, ctx)),
    }
}

fn tr_sizeof(x: &nodes::Sizeof, ctx: &Ctx) -> c::CExpression {
    let argument = &x.argument;
    let c = *argument.clone();
    let arg = match c {
        nodes::SizeofArgument::Expression(x) => c::CSizeofArgument::Expression(tr_expr(&x, ctx)),
        nodes::SizeofArgument::Typename(x) => c::CSizeofArgument::Typename(tr_typename(&x, ctx)),
    };
    c::CExpression::Sizeof {
        argument: Box::new(arg),
    }
}

// f(args)
fn tr_call(x: &nodes::FunctionCall, ctx: &Ctx) -> c::CExpression {
    let f = &x.function;
    let args = &x.arguments;
    let mut targs: Vec<c::CExpression> = Vec::new();
    for arg in args {
        targs.push(tr_expr(arg, ctx))
    }
    c::CExpression::FunctionCall {
        function: Box::new(tr_expr(f, ctx)),
        arguments: targs,
    }
}

fn translate_anonymous_typeform(x: &nodes::AnonymousTypeform, ctx: &Ctx) -> c::CAnonymousTypeform {
    return c::CAnonymousTypeform {
        type_name: tr_typename(&x.type_name, ctx),
        ops: x.ops.clone(),
    };
}

fn tr_func_params(node: &nodes::FunctionParameters, ctx: &Ctx) -> c::CompatFunctionParameters {
    // One parameter, like in structs, is one type and multiple entries,
    // while the C parameters are always one type and one entry.
    let mut parameters: Vec<c::CTypeForm> = Vec::new();
    for parameter in &node.list {
        for form in &parameter.forms {
            parameters.push(c::CTypeForm {
                type_name: tr_typename(&parameter.type_name, ctx),
                form: tr_form(form, ctx),
            })
        }
    }
    return c::CompatFunctionParameters {
        list: parameters,
        variadic: node.variadic,
    };
}

fn tr_func_decl(x: &nodes::FunctionDeclaration, ctx: &Ctx) -> Vec<c::ModElem> {
    let tbody = tr_body(&x.body, ctx);
    let mut rbody = c::CBody {
        statements: Vec::new(),
    };
    // rbody
    //     .statements
    //     .push(nodes_c::CStatement::nodes::Expression(nodes_c::CExpression::FunctionCall {
    //         function: Box::new(nodes_c::CExpression::Identifier(String::from("puts"))),
    //         arguments: vec![nodes_c::CExpression::Literal(nodes_c::CLiteral::String(format!(
    //             "{}:{}",
    //             ctx.path,
    //             format_che::format_form(&form)
    //         )))],
    //     }));
    for s in tbody.statements {
        rbody.statements.push(s);
    }
    let mut r = vec![c::ModElem::DefFunc(c::FunctionDef {
        is_static: !x.is_pub,
        type_name: tr_typename(&x.type_name, ctx),
        form: tr_form(&x.form, ctx),
        parameters: tr_func_params(&x.parameters, ctx),
        body: rbody,
    })];
    if format_che::fmt_form(&x.form) != "main" {
        r.push(c::ModElem::ForwardFunc(c::ForwardFunc {
            is_static: !x.is_pub,
            type_name: tr_typename(&x.type_name, ctx),
            form: tr_form(&x.form, ctx),
            parameters: tr_func_params(&x.parameters, ctx),
        }));
    }
    return r;
}

fn tr_typename(t: &nodes::Typename, ctx: &Ctx) -> c::Typename {
    return c::Typename {
        is_const: t.is_const,
        name: tr_ns_name(&t.name, ctx),
    };
}

// foo.bar() -> _foo_123__bar()
fn tr_ns_name(nsid: &nodes::NsName, ctx: &Ctx) -> String {
    // "OS" is a special namespace that is simply omitted.
    if nsid.namespace == "OS" {
        return nsid.name.clone();
    }
    if nsid.namespace != "" {
        // Get the module that was imported here as this namespace
        // and replace with the module's "id".
        let dep = ctx.deps.iter().find(|d| d.ns == nsid.namespace).unwrap();
        let id = module_gid(&dep.path);
        return format!("{}__{}", id, nsid.name);
    }
    return nsid.name.clone();
}

fn tr_for(x: &nodes::For, ctx: &Ctx) -> c::CStatement {
    let init = &x.init;
    let condition = &x.condition;
    let action = &x.action;
    let body = &x.body;
    let init1 = match init {
        Some(init) => Some(match &init {
            nodes::ForInit::Expression(x) => c::CForInit::Expression(tr_expr(x, ctx)),
            nodes::ForInit::LoopCounterDeclaration {
                type_name,
                form,
                value,
            } => c::CForInit::LoopCounterDeclaration(c::VarDecl {
                type_name: tr_typename(type_name, ctx),
                form: tr_form(form, ctx),
                value: tr_expr(value, ctx),
            }),
        }),
        None => None,
    };
    let condition = match condition {
        Some(x) => Some(tr_expr(&x, ctx)),
        None => None,
    };
    let action = match action {
        Some(x) => Some(tr_expr(&x, ctx)),
        None => None,
    };
    c::CStatement::For {
        init: init1,
        condition,
        action,
        body: tr_body(body, ctx),
    }
}

fn tr_if(x: &nodes::If, ctx: &Ctx) -> c::CStatement {
    let condition = &x.condition;
    let body = &x.body;
    let else_body = &x.else_body;
    c::CStatement::If {
        condition: tr_expr(condition, ctx),
        body: tr_body(body, ctx),
        else_body: else_body.as_ref().map(|x| tr_body(&x, ctx)),
    }
}

fn tr_return(x: &nodes::Return, ctx: &Ctx) -> c::CStatement {
    let expression = &x.expression;
    c::CStatement::Return {
        expression: expression.as_ref().map(|e| tr_expr(&e, ctx)),
    }
}

fn tr_panic(x: &nodes::Panic, ctx: &Ctx) -> c::CStatement {
    let arguments = &x.arguments;
    let pos = &x.pos;
    mk_panic(ctx, pos, arguments)
}

fn tr_switch(x: &nodes::Switch, ctx: &Ctx) -> c::CStatement {
    let is_str = &x.is_str;
    let value = &x.value;
    let cases = &x.cases;
    let default = &x.default_case;
    let switchval = tr_expr(value, ctx);
    if *is_str {
        let c0 = &cases[0];
        c::CStatement::If {
            condition: tr_switchstr_cond(ctx, c0, &switchval),
            body: tr_body(&c0.body, ctx),
            else_body: tr_switchstr_else(cases, 1, default, ctx, &switchval),
        }
    } else {
        let mut tcases: Vec<c::CSwitchCase> = Vec::new();
        for c in cases {
            let mut values = Vec::new();
            for v in &c.values {
                let tv = match v {
                    nodes::SwitchCaseValue::Identifier(x) => {
                        c::CSwitchCaseValue::Identifier(tr_ns_name(x, ctx))
                    }
                    nodes::SwitchCaseValue::Literal(x) => {
                        c::CSwitchCaseValue::Literal(tr_literal(x))
                    }
                };
                values.push(tv)
            }
            tcases.push(c::CSwitchCase {
                values,
                body: tr_body(&c.body, ctx),
            })
        }
        c::CStatement::Switch(c::Switch {
            value: switchval,
            cases: tcases,
            default: default.as_ref().map(|x| tr_body(&x, ctx)),
        })
    }
}

fn tr_vardecl(x: &nodes::VariableDeclaration, ctx: &Ctx) -> c::CStatement {
    let mut tforms: Vec<c::CForm> = Vec::new();
    tforms.push(tr_form(&x.form, ctx));
    let mut tvalues: Vec<Option<c::CExpression>> = Vec::new();
    tvalues.push(x.value.as_ref().map(|x| tr_expr(&x, ctx)));
    c::CStatement::VariableDeclaration {
        type_name: tr_typename(&x.type_name, ctx),
        forms: tforms,
        values: tvalues,
    }
}

fn tr_while(x: &nodes::While, ctx: &Ctx) -> c::CStatement {
    let condition = &x.condition;
    let body = &x.body;
    c::CStatement::While {
        condition: tr_expr(condition, ctx),
        body: tr_body(body, ctx),
    }
}

fn tr_switchstr_else(
    cases: &Vec<nodes::SwitchCase>,
    i: usize,
    default: &Option<nodes::Body>,
    ctx: &Ctx,
    switchval: &c::CExpression,
) -> Option<c::CBody> {
    if i == cases.len() {
        return default.as_ref().map(|x| tr_body(x, ctx));
    }
    let c = &cases[i];
    let e = c::CStatement::If {
        condition: tr_switchstr_cond(ctx, c, &switchval),
        body: tr_body(&c.body, ctx),
        else_body: tr_switchstr_else(cases, i + 1, default, ctx, switchval),
    };
    return Some(c::CBody {
        statements: vec![e],
    });
}

fn tr_switchstr_cond(
    ctx: &Ctx,
    sw: &nodes::SwitchCase,
    switchval: &c::CExpression,
) -> c::CExpression {
    let mut args: Vec<c::CExpression> = Vec::new();
    args.push(match &sw.values[0] {
        nodes::SwitchCaseValue::Identifier(ns_name) => {
            c::CExpression::Identifier(tr_ns_name(&ns_name, ctx))
        }
        nodes::SwitchCaseValue::Literal(literal) => c::CExpression::Literal(tr_literal(&literal)),
    });
    args.push(switchval.clone());
    let mut root = mk_neg(mk_call0("strcmp", args));
    let n = sw.values.len();
    for i in 1..n {
        let mut args: Vec<c::CExpression> = Vec::new();
        args.push(match &sw.values[i] {
            nodes::SwitchCaseValue::Identifier(ns_name) => {
                c::CExpression::Identifier(tr_ns_name(&ns_name, ctx))
            }
            nodes::SwitchCaseValue::Literal(literal) => {
                c::CExpression::Literal(tr_literal(&literal))
            }
        });
        args.push(switchval.clone());
        root = mk_or(root, mk_neg(mk_call0("strcmp", args)));
    }
    return root;
}

// "foo", 12, 'a'
fn tr_literal(x: &nodes::Literal) -> c::CLiteral {
    match x {
        nodes::Literal::Char(val) => c::CLiteral::Char(val.clone()),
        nodes::Literal::String(val) => c::CLiteral::String(val.clone()),
        nodes::Literal::Number(val) => c::CLiteral::Number(val.clone()),
        nodes::Literal::Null => c::CLiteral::Null,
    }
}

fn mk_call0(func: &str, args: Vec<c::CExpression>) -> c::CExpression {
    c::CExpression::FunctionCall {
        function: Box::new(c::CExpression::Identifier(String::from(func))),
        arguments: args,
    }
}

fn mk_call(func: &str, args: Vec<c::CExpression>) -> c::CStatement {
    c::CStatement::Expression(mk_call0(func, args))
}

fn mk_panic(ctx: &Ctx, pos: &String, arguments: &Vec<nodes::Expression>) -> c::CStatement {
    let mut args = vec![c::CExpression::Identifier(String::from("stderr"))];
    for arg in arguments {
        args.push(tr_expr(arg, ctx));
    }
    c::CStatement::Block {
        statements: vec![
            mk_call(
                "fprintf",
                vec![
                    c::CExpression::Identifier(String::from("stderr")),
                    c::CExpression::Literal(c::CLiteral::String(String::from(
                        "*** panic at %s ***\\n",
                    ))),
                    c::CExpression::Literal(c::CLiteral::String(pos.clone())),
                ],
            ),
            mk_call("fprintf", args),
            mk_call(
                "fprintf",
                vec![
                    c::CExpression::Identifier(String::from("stderr")),
                    c::CExpression::Literal(c::CLiteral::String(String::from("\\n"))),
                ],
            ),
            mk_call(
                "exit",
                vec![c::CExpression::Literal(c::CLiteral::Number(String::from(
                    "1",
                )))],
            ),
        ],
    }
}

// Makes a negation of the given expression.
fn mk_neg(operand: c::CExpression) -> c::CExpression {
    c::CExpression::PrefixOperator {
        operator: String::from("!"),
        operand: Box::new(operand),
    }
}

fn mk_or(a: c::CExpression, b: c::CExpression) -> c::CExpression {
    c::CExpression::BinaryOp(c::BinaryOp {
        op: String::from("||"),
        a: Box::new(a),
        b: Box::new(b),
    })
}
