use crate::format;
use crate::format_che;
use crate::nodes::*;
use crate::nodes_c::*;
use crate::parser;
use std::collections::HashSet;

pub fn translate(m: &Module) -> CModule {
    // Build the list of modules to link.
    // These are specified using the #link macros.
    let mut link: Vec<String> = Vec::new();
    for node in &m.elements {
        match node {
            ModuleObject::Macro { name, value } => {
                if name == "link" {
                    link.push(value.clone())
                }
            }
            _ => {}
        }
    }

    // Translate each node into a C node. Some nodes are valid C nodes already,
    // but some are not and have to be converted to their "Compat..." analogs.
    let mut elements: Vec<CModuleObject> = Vec::new();
    for element in &m.elements {
        for node in translate_module_object(element, m) {
            elements.push(node)
        }
    }

    // Since we all know what the standard C library is, just include it all
    // everywhere. We could analyze the module contents and add only those
    // headers that are required, but why bother.
    let std = vec![
        "assert", "ctype", "errno", "limits", "math", "stdarg", "stdbool", "stddef", "stdint",
        "stdio", "stdlib", "string", "time", "setjmp",
    ];
    for n in std {
        elements.push(CModuleObject::Include(format!("<{}.h>", n)));
    }
    elements.push(CModuleObject::Macro {
        name: "define".to_string(),
        value: "nelem(x) (sizeof (x)/sizeof (x)[0])".to_string(),
    });

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
                let s = format::format_typedef(&type_name, &form);
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
                let s = format::format_compat_struct_definition(name, fields);
                if set.contains(&s) {
                    continue;
                }
                set.insert(s);
            }
            _ => {}
        }
        groups[order].push(element);
    }
    let mut sorted_elements: Vec<CModuleObject> = vec![CModuleObject::Split {
        text: String::from(format!("/* -------{}------- */", &m.id)),
    }];
    for group in groups {
        if group.len() == 0 {
            continue;
        }
        sorted_elements.push(CModuleObject::Split {
            text: String::from("/* -------------- */"),
        });
        for e in group {
            sorted_elements.push(e)
        }
    }

    return CModule {
        id: m.id.clone(),
        elements: sorted_elements,
        link,
    };
}

fn translate_module_object(element: &ModuleObject, m: &Module) -> Vec<CModuleObject> {
    match element {
        ModuleObject::Typedef(Typedef {
            is_pub,
            type_name,
            form,
        }) => vec![CModuleObject::Typedef {
            is_pub: *is_pub,
            type_name: translate_typename(type_name),
            form: translate_typedef_form(form),
        }],
        ModuleObject::StructAliasTypedef {
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
        ModuleObject::FuncTypedef(FuncTypedef {
            is_pub,
            return_type,
            name,
            params,
        }) => {
            let mut forms: Vec<CAnonymousTypeform> = Vec::new();
            for f in &params.forms {
                forms.push(translate_anonymous_typeform(&f));
            }
            vec![CModuleObject::FuncTypedef {
                is_pub: *is_pub,
                return_type: translate_typename(return_type),
                name: name.clone(),
                params: CAnonymousParameters {
                    ellipsis: params.ellipsis,
                    forms,
                },
            }]
        }
        ModuleObject::StructTypedef(StructTypedef {
            is_pub,
            fields,
            name,
        }) => {
            // A sugary "typedef {int a} foo_t" is translated to
            // "struct __foo_t_struct {int a}; typedef __foo_t_struct foo_t;".
            // And remember that the struct definition itself is sugar that
            // should be translated as well.
            let struct_name = format!("__{}_struct", name);

            // Build the compat struct fields.
            let mut compat_fields: Vec<CStructItem> = Vec::new();
            for entry in fields {
                match entry {
                    // One fieldlist is multiple fields of the same type.
                    StructEntry::Plain(x) => {
                        for f in &x.forms {
                            compat_fields.push(CStructItem::Field(CTypeForm {
                                type_name: translate_typename(&x.type_name),
                                form: translate_form(f),
                            }));
                        }
                    }
                    StructEntry::Union(x) => {
                        compat_fields.push(CStructItem::Union(translate_union(x)));
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
                        alias: name.clone(),
                    },
                },
            ]
        }
        ModuleObject::Import { path } => {
            let module = parser::get_module(&path, &m.source_path).unwrap();
            let compat = translate(&module);
            return get_module_synopsis(compat);
        }
        ModuleObject::FunctionDeclaration(FunctionDeclaration {
            is_pub,
            type_name,
            form,
            parameters,
            body,
        }) => translate_function_declaration(*is_pub, type_name, form, parameters, body),
        ModuleObject::Enum { is_pub, members } => {
            let mut tm: Vec<CEnumItem> = Vec::new();
            for m in members {
                tm.push(CEnumItem {
                    id: m.id.clone(),
                    value: m.value.as_ref().map(|v| translate_expression(&v)),
                })
            }
            return vec![CModuleObject::EnumDefinition {
                members: tm,
                is_hidden: !is_pub,
            }];
        }
        ModuleObject::Macro { name, value } => {
            if name == "type" || name == "link" {
                return vec![];
            } else {
                return vec![CModuleObject::Macro {
                    name: name.clone(),
                    value: value.clone(),
                }];
            }
        }
        ModuleObject::ModuleVariable {
            form,
            type_name,
            value,
        } => vec![CModuleObject::ModuleVariable {
            type_name: translate_typename(type_name),
            form: translate_form(form),
            value: translate_expression(value),
        }],
        // ModuleObject::CompatInclude(x) => vec![CompatModuleObject::CompatInclude(x.clone())],
    }
}

fn translate_form(f: &Form) -> CForm {
    let mut indexes: Vec<Option<CExpression>> = vec![];
    for index in &f.indexes {
        indexes.push(match index {
            Some(e) => Some(translate_expression(&e)),
            None => None,
        })
    }
    return CForm {
        indexes,
        name: f.name.clone(),
        stars: f.stars.clone(),
    };
}

fn translate_union(x: &Union) -> CUnion {
    let mut fields: Vec<CUnionField> = Vec::new();
    for f in &x.fields {
        fields.push(CUnionField {
            type_name: translate_typename(&f.type_name),
            form: translate_form(&f.form),
        })
    }
    return CUnion {
        form: translate_form(&x.form),
        fields,
    };
}

fn translate_expression(e: &Expression) -> CExpression {
    return match e {
        Expression::ArrayIndex { array, index } => CExpression::ArrayIndex {
            array: Box::new(translate_expression(array)),
            index: Box::new(translate_expression(index)),
        },
        Expression::BinaryOp { op, a, b } => CExpression::BinaryOp {
            op: op.clone(),
            a: Box::new(translate_expression(a)),
            b: Box::new(translate_expression(b)),
        },
        Expression::CompositeLiteral(x) => {
            let mut entries: Vec<CCompositeLiteralEntry> = Vec::new();
            for e in &x.entries {
                entries.push(CCompositeLiteralEntry {
                    is_index: e.is_index,
                    key: e.key.as_ref().map(|x| translate_expression(&x)),
                    value: translate_expression(&e.value),
                })
            }
            CExpression::CompositeLiteral(CCompositeLiteral { entries })
        }
        Expression::Literal(x) => CExpression::Literal(CLiteral {
            type_name: x.type_name.clone(),
            value: x.value.clone(),
        }),
        Expression::Identifier(x) => CExpression::Identifier(x.clone()),
        Expression::PrefixOperator { operator, operand } => CExpression::PrefixOperator {
            operator: operator.clone(),
            operand: Box::new(translate_expression(operand)),
        },
        Expression::PostfixOperator { operator, operand } => CExpression::PostfixOperator {
            operator: operator.clone(),
            operand: Box::new(translate_expression(operand)),
        },
        Expression::Cast { type_name, operand } => CExpression::Cast {
            type_name: translate_anonymous_typeform(type_name),
            operand: Box::new(translate_expression(operand)),
        },
        Expression::FunctionCall {
            function,
            arguments,
        } => {
            let mut targs: Vec<CExpression> = Vec::new();
            for arg in arguments {
                targs.push(translate_expression(arg))
            }
            CExpression::FunctionCall {
                function: Box::new(translate_expression(function)),
                arguments: targs,
            }
        }
        Expression::Sizeof { argument } => {
            let c = *argument.clone();
            let arg = match c {
                SizeofArgument::Expression(x) => {
                    CSizeofArgument::Expression(translate_expression(&x))
                }
                SizeofArgument::Typename(x) => CSizeofArgument::Typename(translate_typename(&x)),
            };
            CExpression::Sizeof {
                argument: Box::new(arg),
            }
        }
    };
}

fn translate_anonymous_typeform(x: &AnonymousTypeform) -> CAnonymousTypeform {
    return CAnonymousTypeform {
        type_name: translate_typename(&x.type_name),
        ops: x.ops.clone(),
    };
}

fn translate_typedef_form(x: &TypedefForm) -> CTypedefForm {
    return CTypedefForm {
        stars: x.stars.clone(),
        params: x.params.as_ref().map(|x| {
            let mut forms: Vec<CAnonymousTypeform> = Vec::new();
            for f in &x.forms {
                forms.push(translate_anonymous_typeform(&f));
            }
            CAnonymousParameters {
                ellipsis: x.ellipsis,
                forms,
            }
        }),
        size: x.size,
        alias: x.alias.clone(),
    };
}

// Module synopsis is what you would usually extract into a header file:
// function prototypes, typedefs, struct declarations.
fn get_module_synopsis(module: CModule) -> Vec<CModuleObject> {
    let mut elements: Vec<CModuleObject> = vec![];

    for element in module.elements {
        match element {
            CModuleObject::Typedef { is_pub, .. } => {
                if is_pub {
                    elements.push(element.clone())
                }
            }
            CModuleObject::StructDefinition {
                name,
                fields,
                is_pub,
            } => {
                if is_pub {
                    elements.push(CModuleObject::StructDefinition {
                        name: name.clone(),
                        fields: fields.clone(),
                        is_pub,
                    })
                }
            }
            CModuleObject::FunctionForwardDeclaration {
                is_static,
                type_name,
                form,
                parameters,
            } => {
                if !is_static {
                    elements.push(CModuleObject::FunctionForwardDeclaration {
                        is_static,
                        type_name,
                        form,
                        parameters,
                    })
                }
            }
            CModuleObject::Macro { name, value } => {
                elements.push(CModuleObject::Macro { name, value })
            }
            CModuleObject::EnumDefinition { is_hidden, members } => {
                if !is_hidden {
                    elements.push(CModuleObject::EnumDefinition { is_hidden, members })
                }
            }
            _ => {}
        }
    }
    return elements;
}

fn translate_function_parameters(node: &FunctionParameters) -> CompatFunctionParameters {
    // One parameter, like in structs, is one type and multiple entries,
    // while the C parameters are always one type and one entry.
    let mut parameters: Vec<CTypeForm> = Vec::new();
    for parameter in &node.list {
        for form in &parameter.forms {
            parameters.push(CTypeForm {
                type_name: translate_typename(&parameter.type_name),
                form: translate_form(form),
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
) -> Vec<CModuleObject> {
    let mut r = vec![CModuleObject::FunctionDefinition {
        is_static: !is_pub,
        type_name: translate_typename(typename),
        form: translate_form(form),
        parameters: translate_function_parameters(&parameters),
        body: translate_body(body),
    }];
    if format_che::format_form(&form) != "main" {
        r.push(CModuleObject::FunctionForwardDeclaration {
            is_static: !is_pub,
            type_name: translate_typename(typename),
            form: translate_form(form),
            parameters: translate_function_parameters(&parameters),
        });
    }
    return r;
}

fn translate_typename(t: &Typename) -> CTypename {
    return CTypename {
        is_const: t.is_const,
        name: t.name.clone(),
    };
}

fn translate_body(b: &Body) -> CBody {
    let mut statements: Vec<CStatement> = Vec::new();
    for s in &b.statements {
        statements.push(match s {
            Statement::Expression(x) => CStatement::Expression(translate_expression(&x)),
            Statement::For {
                init,
                condition,
                action,
                body,
            } => CStatement::For {
                init: match init {
                    ForInit::Expression(x) => CForInit::Expression(translate_expression(x)),
                    ForInit::LoopCounterDeclaration {
                        type_name,
                        form,
                        value,
                    } => CForInit::LoopCounterDeclaration {
                        type_name: translate_typename(type_name),
                        form: translate_form(form),
                        value: translate_expression(value),
                    },
                },
                condition: translate_expression(condition),
                action: translate_expression(action),
                body: translate_body(body),
            },
            Statement::If {
                condition,
                body,
                else_body,
            } => CStatement::If {
                condition: translate_expression(condition),
                body: translate_body(body),
                else_body: else_body.as_ref().map(|x| translate_body(&x)),
            },
            Statement::Return { expression } => CStatement::Return {
                expression: expression.as_ref().map(|e| translate_expression(&e)),
            },
            Statement::Switch {
                value,
                cases,
                default,
            } => {
                let mut tcases: Vec<CSwitchCase> = Vec::new();
                for c in cases {
                    tcases.push(CSwitchCase {
                        value: match &c.value {
                            SwitchCaseValue::Identifier(x) => {
                                CSwitchCaseValue::Identifier(x.clone())
                            }
                            SwitchCaseValue::Literal(x) => CSwitchCaseValue::Literal(CLiteral {
                                type_name: x.type_name.clone(),
                                value: x.value.clone(),
                            }),
                        },
                        body: translate_body(&c.body),
                    })
                }
                CStatement::Switch {
                    value: translate_expression(value),
                    cases: tcases,
                    default: default.as_ref().map(|x| translate_body(&x)),
                }
            }
            Statement::VariableDeclaration {
                type_name,
                forms,
                values,
            } => {
                let mut tforms: Vec<CForm> = Vec::new();
                for f in forms {
                    tforms.push(translate_form(f))
                }
                let mut tvalues: Vec<Option<CExpression>> = Vec::new();
                for e in values {
                    tvalues.push(e.as_ref().map(|x| translate_expression(&x)));
                }
                CStatement::VariableDeclaration {
                    type_name: translate_typename(type_name),
                    forms: tforms,
                    values: tvalues,
                }
            }
            Statement::While { condition, body } => CStatement::While {
                condition: translate_expression(condition),
                body: translate_body(body),
            },
        });
    }
    return CBody { statements };
}
