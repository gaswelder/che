use crate::build::Ctx;
use crate::format_c;
use crate::format_che;
use crate::nodes;
use crate::c;
use std::collections::HashSet;

// Generates a unique identifier prefix for the given module path.
pub fn module_gid(path: &String) -> String {
    return format!(
        "ns_{}",
        path.replace("/", "_").replace(".", "_").replace("-", "_")
    );
}

// Translates a module to c module.
pub fn translate(m: &nodes::Module, ctx: &Ctx) -> c::CModule {
    // Build the list of modules to link.
    // These are specified using the #link macros.
    let mut link: Vec<String> = Vec::new();
    for node in &m.elements {
        match node {
            nodes::ModuleObject::Macro {
                name,
                value,
                pos: _,
            } => {
                if name == "link" {
                    link.push(value.clone())
                }
            }
            _ => {}
        }
    }

    let mut elements: Vec<c::CModuleObject> = Vec::new();
    for element in &m.elements {
        for node in translate_module_object(element, ctx) {
            elements.push(node)
        }
    }

    // Reorder the elements so that typedefs and similar preamble elements
    // come first.
    let mut groups: Vec<Vec<c::CModuleObject>> =
        vec![vec![], vec![], vec![], vec![], vec![], vec![], vec![]];
    let mut set: HashSet<String> = HashSet::new();
    for element in elements {
        let order = match element {
            c::CModuleObject::Include(_) => 0,
            c::CModuleObject::Macro { .. } => 0,
            c::CModuleObject::StructForwardDeclaration(_) => 1,
            c::CModuleObject::Typedef { .. } => 2,
            c::CModuleObject::StructDefinition { .. } => 3,
            c::CModuleObject::EnumDefinition { .. } => 3,
            c::CModuleObject::FunctionForwardDeclaration { .. } => 4,
            c::CModuleObject::ModuleVariable { .. } => 5,
            _ => 6,
        };

        match &element {
            c::CModuleObject::Typedef {
                type_name, form, ..
            } => {
                let s = format_c::format_typedef(&type_name, &form);
                if set.contains(&s) {
                    continue;
                }
                set.insert(s);
            }
            c::CModuleObject::StructDefinition {
                name,
                fields,
                is_pub: _,
            } => {
                let s = format_c::format_compat_struct_definition(name, fields);
                if set.contains(&s) {
                    continue;
                }
                set.insert(s);
            }
            _ => {}
        }
        groups[order].push(element);
    }
    let mut sorted_elements: Vec<c::CModuleObject> = vec![];
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

fn translate_module_object(
    element: &nodes::ModuleObject,
    ctx: &Ctx,
) -> Vec<c::CModuleObject> {
    match element {
        // Ignore import nodes, the upper-lever build function will do all
        // linking and synopses.
        nodes::ModuleObject::Import(_) => Vec::new(),
        nodes::ModuleObject::Typedef(nodes::Typedef {
            pos: _,
            is_pub,
            type_name,
            alias,
            array_size,
            dereference_count,
            function_parameters,
        }) => vec![c::CModuleObject::Typedef {
            is_pub: *is_pub,
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
        }],
        nodes::ModuleObject::StructAliasTypedef {
            pos: _,
            is_pub,
            struct_name,
            type_alias,
        } => vec![c::CModuleObject::Typedef {
            is_pub: *is_pub,
            type_name: c::CTypename {
                is_const: false,
                name: format!("struct {}", struct_name),
            },
            form: c::CTypedefForm {
                stars: String::new(),
                params: None,
                size: 0,
                alias: type_alias.clone(),
            },
        }],
        nodes::ModuleObject::StructTypedef(nodes::StructTypedef {
            pos: _,
            is_pub,
            fields,
            name,
        }) => {
            // A sugary "typedef {int a} foo_t" is translated to
            // "struct __foo_t_struct {int a}; typedef __foo_t_struct foo_t;".
            // And remember that the struct definition itself is sugar that
            // should be translated as well.
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
                                form: translate_form(f, ctx),
                            }));
                        }
                    }
                    nodes::StructEntry::Union(x) => {
                        compat_fields.push(c::CStructItem::Union(translate_union(x, ctx)));
                    }
                }
            }
            vec![
                c::CModuleObject::StructForwardDeclaration(struct_name.clone()),
                c::CModuleObject::StructDefinition {
                    name: struct_name.clone(),
                    fields: compat_fields,
                    is_pub: *is_pub,
                },
                c::CModuleObject::Typedef {
                    is_pub: *is_pub,
                    type_name: c::CTypename {
                        is_const: false,
                        name: format!("struct {}", struct_name.clone()),
                    },
                    form: c::CTypedefForm {
                        stars: "".to_string(),
                        size: 0,
                        params: None,
                        alias: name.name.clone(),
                    },
                },
            ]
        }
        // nodes::ModuleObject::Import { .. } => {
        //     return Vec::new();
        // }
        nodes::ModuleObject::FunctionDeclaration(nodes::FunctionDeclaration {
            is_pub,
            type_name,
            form,
            parameters,
            body,
            pos: _,
        }) => translate_function_declaration(*is_pub, type_name, form, parameters, body, ctx),
        nodes::ModuleObject::Enum {
            is_pub,
            members,
            pos: _,
        } => {
            let mut tm: Vec<c::CEnumItem> = Vec::new();
            for member in members {
                tm.push(c::CEnumItem {
                    id: member.id.name.clone(),
                    value: member.value.as_ref().map(|v| tr_expr(&v, ctx)),
                })
            }
            return vec![c::CModuleObject::EnumDefinition {
                members: tm,
                is_hidden: !is_pub,
            }];
        }
        nodes::ModuleObject::Macro {
            name,
            value,
            pos: _,
        } => {
            if name == "type" || name == "link" {
                return vec![];
            } else {
                return vec![c::CModuleObject::Macro {
                    name: name.clone(),
                    value: value.clone(),
                }];
            }
        }
        nodes::ModuleObject::ModuleVariable(x) => vec![c::CModuleObject::ModuleVariable {
            type_name: tr_typename(&x.type_name, ctx),
            form: translate_form(&x.form, ctx),
            value: tr_expr(x.value.as_ref().unwrap(), ctx),
        }],
    }
}

fn translate_form(f: &nodes::Form, ctx: &Ctx) -> c::CForm {
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

fn translate_union(x: &nodes::Union, ctx: &Ctx) -> c::CUnion {
    let mut fields: Vec<c::CUnionField> = Vec::new();
    for f in &x.fields {
        fields.push(c::CUnionField {
            type_name: tr_typename(&f.type_name, ctx),
            form: translate_form(&f.form, ctx),
        })
    }
    return c::CUnion {
        form: translate_form(&x.form, ctx),
        fields,
    };
}

fn tr_expr(e: &nodes::Expression, ctx: &Ctx) -> c::CExpression {
    return match e {
        nodes::Expression::NsName(n) => c::CExpression::Identifier(tr_ns_name(n, ctx)),
        nodes::Expression::ArrayIndex { array, index } => c::CExpression::ArrayIndex {
            array: Box::new(tr_expr(array, ctx)),
            index: Box::new(tr_expr(index, ctx)),
        },
        nodes::Expression::BinaryOp { op, a, b } => c::CExpression::BinaryOp {
            op: op.clone(),
            a: Box::new(tr_expr(a, ctx)),
            b: Box::new(tr_expr(b, ctx)),
        },
        nodes::Expression::FieldAccess {
            op,
            target,
            field_name,
        } => c::CExpression::FieldAccess {
            op: op.clone(),
            target: Box::new(tr_expr(target, ctx)),
            field_name: field_name.name.clone(),
        },
        nodes::Expression::CompositeLiteral(x) => {
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
        nodes::Expression::Literal(x) => c::CExpression::Literal(tr_literal(x)),
        nodes::Expression::Identifier(x) => c::CExpression::Identifier(x.name.clone()),
        nodes::Expression::PrefixOperator { operator, operand } => {
            c::CExpression::PrefixOperator {
                operator: operator.clone(),
                operand: Box::new(tr_expr(operand, ctx)),
            }
        }
        nodes::Expression::PostfixOperator { operator, operand } => {
            c::CExpression::PostfixOperator {
                operator: operator.clone(),
                operand: Box::new(tr_expr(operand, ctx)),
            }
        }
        nodes::Expression::Cast { type_name, operand } => c::CExpression::Cast {
            type_name: translate_anonymous_typeform(type_name, ctx),
            operand: Box::new(tr_expr(operand, ctx)),
        },
        nodes::Expression::FunctionCall {
            function,
            arguments,
        } => tr_call(function, arguments, ctx),
        nodes::Expression::Sizeof { argument } => {
            let c = *argument.clone();
            let arg = match c {
                nodes::SizeofArgument::Expression(x) => {
                    c::CSizeofArgument::Expression(tr_expr(&x, ctx))
                }
                nodes::SizeofArgument::Typename(x) => {
                    c::CSizeofArgument::Typename(tr_typename(&x, ctx))
                }
            };
            c::CExpression::Sizeof {
                argument: Box::new(arg),
            }
        }
    };
}

// f(args)
fn tr_call(
    f: &Box<nodes::Expression>,
    args: &Vec<nodes::Expression>,
    ctx: &Ctx,
) -> c::CExpression {
    let mut targs: Vec<c::CExpression> = Vec::new();
    for arg in args {
        targs.push(tr_expr(arg, ctx))
    }
    c::CExpression::FunctionCall {
        function: Box::new(tr_expr(f, ctx)),
        arguments: targs,
    }
}

fn gencall0(func: &str, args: Vec<c::CExpression>) -> c::CExpression {
    c::CExpression::FunctionCall {
        function: Box::new(c::CExpression::Identifier(String::from(func))),
        arguments: args,
    }
}

fn gencall(func: &str, args: Vec<c::CExpression>) -> c::CStatement {
    c::CStatement::Expression(gencall0(func, args))
}

fn generate_panic(
    ctx: &Ctx,
    pos: &String,
    arguments: &Vec<nodes::Expression>,
) -> c::CStatement {
    let mut args = vec![c::CExpression::Identifier(String::from("stderr"))];
    for arg in arguments {
        args.push(tr_expr(arg, ctx));
    }
    c::CStatement::Block {
        statements: vec![
            gencall(
                "fprintf",
                vec![
                    c::CExpression::Identifier(String::from("stderr")),
                    c::CExpression::Literal(c::CLiteral::String(String::from(
                        "*** panic at %s ***\\n",
                    ))),
                    c::CExpression::Literal(c::CLiteral::String(pos.clone())),
                ],
            ),
            gencall("fprintf", args),
            gencall(
                "fprintf",
                vec![
                    c::CExpression::Identifier(String::from("stderr")),
                    c::CExpression::Literal(c::CLiteral::String(String::from("\\n"))),
                ],
            ),
            gencall(
                "exit",
                vec![c::CExpression::Literal(c::CLiteral::Number(
                    String::from("1"),
                ))],
            ),
        ],
    }
}

fn translate_anonymous_typeform(
    x: &nodes::AnonymousTypeform,
    ctx: &Ctx,
) -> c::CAnonymousTypeform {
    return c::CAnonymousTypeform {
        type_name: tr_typename(&x.type_name, ctx),
        ops: x.ops.clone(),
    };
}

// Module synopsis is what you would usually extract into a header file:
// function prototypes, typedefs, struct declarations.
pub fn get_module_synopsis(module: &c::CModule) -> Vec<c::CModuleObject> {
    let mut elements: Vec<c::CModuleObject> = vec![];

    for element in &module.elements {
        match element {
            c::CModuleObject::Typedef {
                is_pub,
                form,
                type_name,
            } => {
                if *is_pub {
                    elements.push(c::CModuleObject::Typedef {
                        is_pub: true,
                        type_name: type_name.clone(),
                        form: form.clone(),
                    })
                }
            }
            c::CModuleObject::StructDefinition {
                name,
                fields,
                is_pub,
            } => {
                if *is_pub {
                    elements.push(c::CModuleObject::StructDefinition {
                        name: name.clone(),
                        fields: fields.clone(),
                        is_pub: *is_pub,
                    })
                }
            }
            c::CModuleObject::FunctionForwardDeclaration {
                is_static,
                type_name,
                form,
                parameters,
            } => {
                if !*is_static {
                    elements.push(c::CModuleObject::FunctionForwardDeclaration {
                        is_static: *is_static,
                        type_name: type_name.clone(),
                        form: form.clone(),
                        parameters: parameters.clone(),
                    })
                }
            }
            c::CModuleObject::Macro { name, value } => {
                // if name == "include" {
                elements.push(c::CModuleObject::Macro {
                    name: name.clone(),
                    value: value.clone(),
                })
                // }
            }
            c::CModuleObject::EnumDefinition { is_hidden, members } => {
                if !*is_hidden {
                    elements.push(c::CModuleObject::EnumDefinition {
                        is_hidden: *is_hidden,
                        members: members.clone(),
                    })
                }
            }
            _ => {}
        }
    }
    return elements;
}

fn tr_func_params(
    node: &nodes::FunctionParameters,
    ctx: &Ctx,
) -> c::CompatFunctionParameters {
    // One parameter, like in structs, is one type and multiple entries,
    // while the C parameters are always one type and one entry.
    let mut parameters: Vec<c::CTypeForm> = Vec::new();
    for parameter in &node.list {
        for form in &parameter.forms {
            parameters.push(c::CTypeForm {
                type_name: tr_typename(&parameter.type_name, ctx),
                form: translate_form(form, ctx),
            })
        }
    }
    return c::CompatFunctionParameters {
        list: parameters,
        variadic: node.variadic,
    };
}

fn translate_function_declaration(
    is_pub: bool,
    typename: &nodes::Typename,
    form: &nodes::Form,
    parameters: &nodes::FunctionParameters,
    body: &nodes::Body,
    ctx: &Ctx,
) -> Vec<c::CModuleObject> {
    let tbody = translate_body(body, ctx);
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
    let mut r = vec![c::CModuleObject::FunctionDefinition {
        is_static: !is_pub,
        type_name: tr_typename(typename, ctx),
        form: translate_form(form, ctx),
        parameters: tr_func_params(&parameters, ctx),
        body: rbody,
    }];
    if format_che::format_form(&form) != "main" {
        r.push(c::CModuleObject::FunctionForwardDeclaration {
            is_static: !is_pub,
            type_name: tr_typename(typename, ctx),
            form: translate_form(form, ctx),
            parameters: tr_func_params(&parameters, ctx),
        });
    }
    return r;
}

fn tr_typename(t: &nodes::Typename, ctx: &Ctx) -> c::CTypename {
    return c::CTypename {
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

fn translate_body(b: &nodes::Body, ctx: &Ctx) -> c::CBody {
    let mut statements: Vec<c::CStatement> = Vec::new();
    for s in &b.statements {
        statements.push(match s {
            nodes::Statement::Break => c::CStatement::Break,
            nodes::Statement::Continue => c::CStatement::Continue,
            nodes::Statement::Expression(x) => c::CStatement::Expression(tr_expr(&x, ctx)),
            nodes::Statement::For {
                init,
                condition,
                action,
                body,
            } => {
                let init = match init {
                    Some(init) => Some(match &init {
                        nodes::ForInit::Expression(x) => {
                            c::CForInit::Expression(tr_expr(x, ctx))
                        }
                        nodes::ForInit::LoopCounterDeclaration {
                            type_name,
                            form,
                            value,
                        } => c::CForInit::LoopCounterDeclaration {
                            type_name: tr_typename(type_name, ctx),
                            form: translate_form(form, ctx),
                            value: tr_expr(value, ctx),
                        },
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
                    init,
                    condition,
                    action,
                    body: translate_body(body, ctx),
                }
            }
            nodes::Statement::If {
                condition,
                body,
                else_body,
            } => c::CStatement::If {
                condition: tr_expr(condition, ctx),
                body: translate_body(body, ctx),
                else_body: else_body.as_ref().map(|x| translate_body(&x, ctx)),
            },
            nodes::Statement::Return { expression } => c::CStatement::Return {
                expression: expression.as_ref().map(|e| tr_expr(&e, ctx)),
            },
            nodes::Statement::Panic { arguments, pos } => generate_panic(ctx, pos, arguments),
            nodes::Statement::Switch {
                is_str,
                value,
                cases,
                default_case: default,
            } => {
                let switchval = tr_expr(value, ctx);
                if *is_str {
                    let c0 = &cases[0];
                    c::CStatement::If {
                        condition: translate_switch_str_cond(ctx, c0, &switchval),
                        body: translate_body(&c0.body, ctx),
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
                            body: translate_body(&c.body, ctx),
                        })
                    }
                    c::CStatement::Switch {
                        value: switchval,
                        cases: tcases,
                        default: default.as_ref().map(|x| translate_body(&x, ctx)),
                    }
                }
            }
            nodes::Statement::VariableDeclaration(x) => {
                let mut tforms: Vec<c::CForm> = Vec::new();
                tforms.push(translate_form(&x.form, ctx));
                let mut tvalues: Vec<Option<c::CExpression>> = Vec::new();
                tvalues.push(x.value.as_ref().map(|x| tr_expr(&x, ctx)));
                c::CStatement::VariableDeclaration {
                    type_name: tr_typename(&x.type_name, ctx),
                    forms: tforms,
                    values: tvalues,
                }
            }
            nodes::Statement::While { condition, body } => c::CStatement::While {
                condition: tr_expr(condition, ctx),
                body: translate_body(body, ctx),
            },
        });
    }
    return c::CBody { statements };
}

fn tr_switchstr_else(
    cases: &Vec<nodes::SwitchCase>,
    i: usize,
    default: &Option<nodes::Body>,
    ctx: &Ctx,
    switchval: &c::CExpression,
) -> Option<c::CBody> {
    if i == cases.len() {
        return default.as_ref().map(|x| translate_body(x, ctx));
    }
    let c = &cases[i];
    let e = c::CStatement::If {
        condition: translate_switch_str_cond(ctx, c, &switchval),
        body: translate_body(&c.body, ctx),
        else_body: tr_switchstr_else(cases, i + 1, default, ctx, switchval),
    };
    return Some(c::CBody {
        statements: vec![e],
    });
}

// Makes a negation of the given expression.
fn mk_neg(operand: c::CExpression) -> c::CExpression {
    c::CExpression::PrefixOperator {
        operator: String::from("!"),
        operand: Box::new(operand),
    }
}

fn mk_or(a: c::CExpression, b: c::CExpression) -> c::CExpression {
    c::CExpression::BinaryOp {
        op: String::from("||"),
        a: Box::new(a),
        b: Box::new(b),
    }
}

fn translate_switch_str_cond(
    ctx: &Ctx,
    sw: &nodes::SwitchCase,
    switchval: &c::CExpression,
) -> c::CExpression {
    let mut args: Vec<c::CExpression> = Vec::new();
    args.push(match &sw.values[0] {
        nodes::SwitchCaseValue::Identifier(ns_name) => {
            c::CExpression::Identifier(tr_ns_name(&ns_name, ctx))
        }
        nodes::SwitchCaseValue::Literal(literal) => {
            c::CExpression::Literal(tr_literal(&literal))
        }
    });
    args.push(switchval.clone());
    let mut root = mk_neg(gencall0("strcmp", args));
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
        root = mk_or(root, mk_neg(gencall0("strcmp", args)));
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
