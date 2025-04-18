use crate::build::Ctx;
use crate::format_c;
use crate::format_che;
use crate::nodes::*;
use crate::nodes_c::*;
use std::collections::HashSet;

pub fn module_gid(path: &String) -> String {
    return format!(
        "ns_{}",
        path.replace("/", "_").replace(".", "_").replace("-", "_")
    );
}

pub fn translate(m: &Module, ctx: &Ctx) -> CModule {
    // Build the list of modules to link.
    // These are specified using the #link macros.
    let mut link: Vec<String> = Vec::new();
    for node in &m.elements {
        match node {
            ModuleObject::Macro {
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

    let mut elements: Vec<CModuleObject> = Vec::new();
    for element in &m.elements {
        for node in translate_module_object(element, ctx) {
            elements.push(node)
        }
    }

    // Reorder the elements so that typedefs and similar preamble elements
    // come first.
    let mut groups: Vec<Vec<CModuleObject>> =
        vec![vec![], vec![], vec![], vec![], vec![], vec![], vec![]];
    let mut set: HashSet<String> = HashSet::new();
    for element in elements {
        let order = match element {
            CModuleObject::Include(_) => 0,
            CModuleObject::Macro { .. } => 0,
            CModuleObject::StructForwardDeclaration(_) => 1,
            CModuleObject::Typedef { .. } => 2,
            CModuleObject::StructDefinition { .. } => 3,
            CModuleObject::EnumDefinition { .. } => 3,
            CModuleObject::FunctionForwardDeclaration { .. } => 4,
            CModuleObject::ModuleVariable { .. } => 5,
            _ => 6,
        };

        match &element {
            CModuleObject::Typedef {
                type_name, form, ..
            } => {
                let s = format_c::format_typedef(&type_name, &form);
                if set.contains(&s) {
                    continue;
                }
                set.insert(s);
            }
            CModuleObject::StructDefinition {
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
    let mut sorted_elements: Vec<CModuleObject> = vec![];
    for group in groups {
        for e in group {
            sorted_elements.push(e)
        }
    }
    return CModule {
        elements: sorted_elements,
        link,
    };
}

fn translate_module_object(element: &ModuleObject, ctx: &Ctx) -> Vec<CModuleObject> {
    match element {
        // Ignore import nodes, the upper-lever build function will do all
        // linking and synopses.
        ModuleObject::Import(_) => Vec::new(),
        ModuleObject::Typedef(Typedef {
            pos: _,
            is_pub,
            type_name,
            alias,
            array_size,
            dereference_count,
            function_parameters,
        }) => vec![CModuleObject::Typedef {
            is_pub: *is_pub,
            type_name: translate_typename(type_name, ctx),
            form: CTypedefForm {
                stars: "*".repeat(*dereference_count),
                params: function_parameters.as_ref().map(|x| {
                    let mut forms: Vec<CAnonymousTypeform> = Vec::new();
                    for f in &x.forms {
                        forms.push(translate_anonymous_typeform(&f, ctx));
                    }
                    CAnonymousParameters {
                        ellipsis: x.ellipsis,
                        forms,
                    }
                }),
                size: *array_size,
                alias: alias.name.clone(),
            },
        }],
        ModuleObject::StructAliasTypedef {
            pos: _,
            is_pub,
            struct_name,
            type_alias,
        } => vec![CModuleObject::Typedef {
            is_pub: *is_pub,
            type_name: CTypename {
                is_const: false,
                name: format!("struct {}", struct_name),
            },
            form: CTypedefForm {
                stars: String::new(),
                params: None,
                size: 0,
                alias: type_alias.clone(),
            },
        }],
        ModuleObject::StructTypedef(StructTypedef {
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
            let mut compat_fields: Vec<CStructItem> = Vec::new();
            for entry in fields {
                match entry {
                    // One fieldlist is multiple fields of the same type.
                    StructEntry::Plain(x) => {
                        for f in &x.forms {
                            compat_fields.push(CStructItem::Field(CTypeForm {
                                type_name: translate_typename(&x.type_name, ctx),
                                form: translate_form(f, ctx),
                            }));
                        }
                    }
                    StructEntry::Union(x) => {
                        compat_fields.push(CStructItem::Union(translate_union(x, ctx)));
                    }
                }
            }
            vec![
                CModuleObject::StructForwardDeclaration(struct_name.clone()),
                CModuleObject::StructDefinition {
                    name: struct_name.clone(),
                    fields: compat_fields,
                    is_pub: *is_pub,
                },
                CModuleObject::Typedef {
                    is_pub: *is_pub,
                    type_name: CTypename {
                        is_const: false,
                        name: format!("struct {}", struct_name.clone()),
                    },
                    form: CTypedefForm {
                        stars: "".to_string(),
                        size: 0,
                        params: None,
                        alias: name.name.clone(),
                    },
                },
            ]
        }
        // ModuleObject::Import { .. } => {
        //     return Vec::new();
        // }
        ModuleObject::FunctionDeclaration(FunctionDeclaration {
            is_pub,
            type_name,
            form,
            parameters,
            body,
            pos: _,
        }) => translate_function_declaration(*is_pub, type_name, form, parameters, body, ctx),
        ModuleObject::Enum {
            is_pub,
            members,
            pos: _,
        } => {
            let mut tm: Vec<CEnumItem> = Vec::new();
            for member in members {
                tm.push(CEnumItem {
                    id: member.id.name.clone(),
                    value: member.value.as_ref().map(|v| translate_expression(&v, ctx)),
                })
            }
            return vec![CModuleObject::EnumDefinition {
                members: tm,
                is_hidden: !is_pub,
            }];
        }
        ModuleObject::Macro {
            name,
            value,
            pos: _,
        } => {
            if name == "type" || name == "link" {
                return vec![];
            } else {
                return vec![CModuleObject::Macro {
                    name: name.clone(),
                    value: value.clone(),
                }];
            }
        }
        ModuleObject::ModuleVariable(x) => vec![CModuleObject::ModuleVariable {
            type_name: translate_typename(&x.type_name, ctx),
            form: translate_form(&x.form, ctx),
            value: translate_expression(x.value.as_ref().unwrap(), ctx),
        }],
    }
}

fn translate_form(f: &Form, ctx: &Ctx) -> CForm {
    let mut indexes: Vec<Option<CExpression>> = vec![];
    for index in &f.indexes {
        indexes.push(match index {
            Some(e) => Some(translate_expression(&e, ctx)),
            None => None,
        })
    }
    return CForm {
        indexes,
        name: f.name.clone(),
        stars: "*".repeat(f.hops),
    };
}

fn translate_union(x: &Union, ctx: &Ctx) -> CUnion {
    let mut fields: Vec<CUnionField> = Vec::new();
    for f in &x.fields {
        fields.push(CUnionField {
            type_name: translate_typename(&f.type_name, ctx),
            form: translate_form(&f.form, ctx),
        })
    }
    return CUnion {
        form: translate_form(&x.form, ctx),
        fields,
    };
}

fn translate_expression(e: &Expression, ctx: &Ctx) -> CExpression {
    return match e {
        Expression::NsName(n) => CExpression::Identifier(translate_ns_name(n, ctx)),
        Expression::ArrayIndex { array, index } => CExpression::ArrayIndex {
            array: Box::new(translate_expression(array, ctx)),
            index: Box::new(translate_expression(index, ctx)),
        },
        Expression::BinaryOp { op, a, b } => CExpression::BinaryOp {
            op: op.clone(),
            a: Box::new(translate_expression(a, ctx)),
            b: Box::new(translate_expression(b, ctx)),
        },
        Expression::FieldAccess {
            op,
            target,
            field_name,
        } => CExpression::FieldAccess {
            op: op.clone(),
            target: Box::new(translate_expression(target, ctx)),
            field_name: field_name.name.clone(),
        },
        Expression::CompositeLiteral(x) => {
            let mut entries: Vec<CCompositeLiteralEntry> = Vec::new();
            for e in &x.entries {
                entries.push(CCompositeLiteralEntry {
                    is_index: e.is_index,
                    key: e.key.as_ref().map(|x| translate_expression(&x, ctx)),
                    value: translate_expression(&e.value, ctx),
                })
            }
            CExpression::CompositeLiteral(CCompositeLiteral { entries })
        }
        Expression::Literal(x) => CExpression::Literal(translate_literal(x)),
        Expression::Identifier(x) => CExpression::Identifier(x.name.clone()),
        Expression::PrefixOperator { operator, operand } => CExpression::PrefixOperator {
            operator: operator.clone(),
            operand: Box::new(translate_expression(operand, ctx)),
        },
        Expression::PostfixOperator { operator, operand } => CExpression::PostfixOperator {
            operator: operator.clone(),
            operand: Box::new(translate_expression(operand, ctx)),
        },
        Expression::Cast { type_name, operand } => CExpression::Cast {
            type_name: translate_anonymous_typeform(type_name, ctx),
            operand: Box::new(translate_expression(operand, ctx)),
        },
        Expression::FunctionCall {
            function,
            arguments,
        } => translate_function_call(function, arguments, ctx),
        Expression::Sizeof { argument } => {
            let c = *argument.clone();
            let arg = match c {
                SizeofArgument::Expression(x) => {
                    CSizeofArgument::Expression(translate_expression(&x, ctx))
                }
                SizeofArgument::Typename(x) => {
                    CSizeofArgument::Typename(translate_typename(&x, ctx))
                }
            };
            CExpression::Sizeof {
                argument: Box::new(arg),
            }
        }
    };
}

fn translate_function_call(
    function: &Box<Expression>,
    arguments: &Vec<Expression>,
    ctx: &Ctx,
) -> CExpression {
    let mut targs: Vec<CExpression> = Vec::new();
    for arg in arguments {
        targs.push(translate_expression(arg, ctx))
    }
    CExpression::FunctionCall {
        function: Box::new(translate_expression(function, ctx)),
        arguments: targs,
    }
}

fn gencall0(func: &str, args: Vec<CExpression>) -> CExpression {
    CExpression::FunctionCall {
        function: Box::new(CExpression::Identifier(String::from(func))),
        arguments: args,
    }
}

fn gencall(func: &str, args: Vec<CExpression>) -> CStatement {
    CStatement::Expression(gencall0(func, args))
}

fn generate_panic(ctx: &Ctx, pos: &String, arguments: &Vec<Expression>) -> CStatement {
    let mut args = vec![CExpression::Identifier(String::from("stderr"))];
    for arg in arguments {
        args.push(translate_expression(arg, ctx));
    }
    CStatement::Block {
        statements: vec![
            gencall(
                "fprintf",
                vec![
                    CExpression::Identifier(String::from("stderr")),
                    CExpression::Literal(CLiteral::String(String::from("*** panic at %s ***\\n"))),
                    CExpression::Literal(CLiteral::String(pos.clone())),
                ],
            ),
            gencall("fprintf", args),
            gencall(
                "fprintf",
                vec![
                    CExpression::Identifier(String::from("stderr")),
                    CExpression::Literal(CLiteral::String(String::from("\\n"))),
                ],
            ),
            gencall(
                "exit",
                vec![CExpression::Literal(CLiteral::Number(String::from("1")))],
            ),
        ],
    }
}

fn translate_anonymous_typeform(x: &AnonymousTypeform, ctx: &Ctx) -> CAnonymousTypeform {
    return CAnonymousTypeform {
        type_name: translate_typename(&x.type_name, ctx),
        ops: x.ops.clone(),
    };
}

// Module synopsis is what you would usually extract into a header file:
// function prototypes, typedefs, struct declarations.
pub fn get_module_synopsis(module: &CModule) -> Vec<CModuleObject> {
    let mut elements: Vec<CModuleObject> = vec![];

    for element in &module.elements {
        match element {
            CModuleObject::Typedef {
                is_pub,
                form,
                type_name,
            } => {
                if *is_pub {
                    elements.push(CModuleObject::Typedef {
                        is_pub: true,
                        type_name: type_name.clone(),
                        form: form.clone(),
                    })
                }
            }
            CModuleObject::StructDefinition {
                name,
                fields,
                is_pub,
            } => {
                if *is_pub {
                    elements.push(CModuleObject::StructDefinition {
                        name: name.clone(),
                        fields: fields.clone(),
                        is_pub: *is_pub,
                    })
                }
            }
            CModuleObject::FunctionForwardDeclaration {
                is_static,
                type_name,
                form,
                parameters,
            } => {
                if !*is_static {
                    elements.push(CModuleObject::FunctionForwardDeclaration {
                        is_static: *is_static,
                        type_name: type_name.clone(),
                        form: form.clone(),
                        parameters: parameters.clone(),
                    })
                }
            }
            CModuleObject::Macro { name, value } => {
                // if name == "include" {
                elements.push(CModuleObject::Macro {
                    name: name.clone(),
                    value: value.clone(),
                })
                // }
            }
            CModuleObject::EnumDefinition { is_hidden, members } => {
                if !*is_hidden {
                    elements.push(CModuleObject::EnumDefinition {
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

fn translate_function_parameters(node: &FunctionParameters, ctx: &Ctx) -> CompatFunctionParameters {
    // One parameter, like in structs, is one type and multiple entries,
    // while the C parameters are always one type and one entry.
    let mut parameters: Vec<CTypeForm> = Vec::new();
    for parameter in &node.list {
        for form in &parameter.forms {
            parameters.push(CTypeForm {
                type_name: translate_typename(&parameter.type_name, ctx),
                form: translate_form(form, ctx),
            })
        }
    }
    return CompatFunctionParameters {
        list: parameters,
        variadic: node.variadic,
    };
}

fn translate_function_declaration(
    is_pub: bool,
    typename: &Typename,
    form: &Form,
    parameters: &FunctionParameters,
    body: &Body,
    ctx: &Ctx,
) -> Vec<CModuleObject> {
    let tbody = translate_body(body, ctx);
    let mut rbody = CBody {
        statements: Vec::new(),
    };
    // rbody
    //     .statements
    //     .push(CStatement::Expression(CExpression::FunctionCall {
    //         function: Box::new(CExpression::Identifier(String::from("puts"))),
    //         arguments: vec![CExpression::Literal(CLiteral::String(format!(
    //             "{}:{}",
    //             ctx.path,
    //             format_che::format_form(&form)
    //         )))],
    //     }));
    for s in tbody.statements {
        rbody.statements.push(s);
    }
    let mut r = vec![CModuleObject::FunctionDefinition {
        is_static: !is_pub,
        type_name: translate_typename(typename, ctx),
        form: translate_form(form, ctx),
        parameters: translate_function_parameters(&parameters, ctx),
        body: rbody,
    }];
    if format_che::format_form(&form) != "main" {
        r.push(CModuleObject::FunctionForwardDeclaration {
            is_static: !is_pub,
            type_name: translate_typename(typename, ctx),
            form: translate_form(form, ctx),
            parameters: translate_function_parameters(&parameters, ctx),
        });
    }
    return r;
}

fn translate_typename(t: &Typename, ctx: &Ctx) -> CTypename {
    return CTypename {
        is_const: t.is_const,
        name: translate_ns_name(&t.name, ctx),
    };
}

fn translate_ns_name(nsid: &NsName, ctx: &Ctx) -> String {
    if nsid.namespace == "OS" {
        return nsid.name.clone();
    }
    if nsid.namespace != "" {
        // get the module that was imported here as <namespace>.
        let dep = ctx.deps.iter().find(|d| d.ns == nsid.namespace).unwrap();

        // Replace with the module's "id".
        let id = module_gid(&dep.path);
        return format!("{}__{}", id, nsid.name);
    }
    return nsid.name.clone();
}

fn translate_body(b: &Body, ctx: &Ctx) -> CBody {
    let mut statements: Vec<CStatement> = Vec::new();
    for s in &b.statements {
        statements.push(match s {
            Statement::Break => CStatement::Break,
            Statement::Continue => CStatement::Continue,
            Statement::Expression(x) => CStatement::Expression(translate_expression(&x, ctx)),
            Statement::For {
                init,
                condition,
                action,
                body,
            } => {
                let init = match init {
                    Some(init) => Some(match &init {
                        ForInit::Expression(x) => {
                            CForInit::Expression(translate_expression(x, ctx))
                        }
                        ForInit::LoopCounterDeclaration {
                            type_name,
                            form,
                            value,
                        } => CForInit::LoopCounterDeclaration {
                            type_name: translate_typename(type_name, ctx),
                            form: translate_form(form, ctx),
                            value: translate_expression(value, ctx),
                        },
                    }),
                    None => None,
                };
                let condition = match condition {
                    Some(x) => Some(translate_expression(&x, ctx)),
                    None => None,
                };
                let action = match action {
                    Some(x) => Some(translate_expression(&x, ctx)),
                    None => None,
                };
                CStatement::For {
                    init,
                    condition,
                    action,
                    body: translate_body(body, ctx),
                }
            }
            Statement::If {
                condition,
                body,
                else_body,
            } => CStatement::If {
                condition: translate_expression(condition, ctx),
                body: translate_body(body, ctx),
                else_body: else_body.as_ref().map(|x| translate_body(&x, ctx)),
            },
            Statement::Return { expression } => CStatement::Return {
                expression: expression.as_ref().map(|e| translate_expression(&e, ctx)),
            },
            Statement::Panic { arguments, pos } => generate_panic(ctx, pos, arguments),
            Statement::Switch {
                is_str,
                value,
                cases,
                default_case: default,
            } => {
                let switchval = translate_expression(value, ctx);
                if *is_str {
                    let c0 = &cases[0];
                    CStatement::If {
                        condition: translate_switch_str_cond(ctx, c0, &switchval),
                        body: translate_body(&c0.body, ctx),
                        else_body: tr_switchstr_else(cases, 1, default, ctx, &switchval),
                    }
                } else {
                    let mut tcases: Vec<CSwitchCase> = Vec::new();
                    for c in cases {
                        let mut values = Vec::new();
                        for v in &c.values {
                            let tv = match v {
                                SwitchCaseValue::Identifier(x) => {
                                    CSwitchCaseValue::Identifier(translate_ns_name(x, ctx))
                                }
                                SwitchCaseValue::Literal(x) => {
                                    CSwitchCaseValue::Literal(translate_literal(x))
                                }
                            };
                            values.push(tv)
                        }
                        tcases.push(CSwitchCase {
                            values,
                            body: translate_body(&c.body, ctx),
                        })
                    }
                    CStatement::Switch {
                        value: switchval,
                        cases: tcases,
                        default: default.as_ref().map(|x| translate_body(&x, ctx)),
                    }
                }
            }
            Statement::VariableDeclaration(x) => {
                let mut tforms: Vec<CForm> = Vec::new();
                tforms.push(translate_form(&x.form, ctx));
                let mut tvalues: Vec<Option<CExpression>> = Vec::new();
                tvalues.push(x.value.as_ref().map(|x| translate_expression(&x, ctx)));
                CStatement::VariableDeclaration {
                    type_name: translate_typename(&x.type_name, ctx),
                    forms: tforms,
                    values: tvalues,
                }
            }
            Statement::While { condition, body } => CStatement::While {
                condition: translate_expression(condition, ctx),
                body: translate_body(body, ctx),
            },
        });
    }
    return CBody { statements };
}

fn tr_switchstr_else(
    cases: &Vec<SwitchCase>,
    i: usize,
    default: &Option<Body>,
    ctx: &Ctx,
    switchval: &CExpression,
) -> Option<CBody> {
    if i == cases.len() {
        return default.as_ref().map(|x| translate_body(x, ctx));
    }
    let c = &cases[i];
    let e = CStatement::If {
        condition: translate_switch_str_cond(ctx, c, &switchval),
        body: translate_body(&c.body, ctx),
        else_body: tr_switchstr_else(cases, i + 1, default, ctx, switchval),
    };
    return Some(CBody {
        statements: vec![e],
    });
}

fn genneg(operand: CExpression) -> CExpression {
    CExpression::PrefixOperator {
        operator: String::from("!"),
        operand: Box::new(operand),
    }
}

fn genor(a: CExpression, b: CExpression) -> CExpression {
    CExpression::BinaryOp {
        op: String::from("||"),
        a: Box::new(a),
        b: Box::new(b),
    }
}

fn translate_switch_str_cond(ctx: &Ctx, sw: &SwitchCase, switchval: &CExpression) -> CExpression {
    let mut args: Vec<CExpression> = Vec::new();
    args.push(match &sw.values[0] {
        SwitchCaseValue::Identifier(ns_name) => {
            CExpression::Identifier(translate_ns_name(&ns_name, ctx))
        }
        SwitchCaseValue::Literal(literal) => CExpression::Literal(translate_literal(&literal)),
    });
    args.push(switchval.clone());
    let mut root = genneg(gencall0("strcmp", args));
    let n = sw.values.len();
    for i in 1..n {
        let mut args: Vec<CExpression> = Vec::new();
        args.push(match &sw.values[i] {
            SwitchCaseValue::Identifier(ns_name) => {
                CExpression::Identifier(translate_ns_name(&ns_name, ctx))
            }
            SwitchCaseValue::Literal(literal) => CExpression::Literal(translate_literal(&literal)),
        });
        args.push(switchval.clone());
        root = genor(root, genneg(gencall0("strcmp", args)));
    }
    return root;
}

fn translate_literal(x: &Literal) -> CLiteral {
    match x {
        Literal::Char(val) => CLiteral::Char(val.clone()),
        Literal::String(val) => CLiteral::String(val.clone()),
        Literal::Number(val) => CLiteral::Number(val.clone()),
        Literal::Null => CLiteral::Null,
    }
}
