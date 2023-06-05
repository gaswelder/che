use crate::format;
use crate::nodes::*;
use crate::parser;
use std::collections::HashSet;

pub fn translate(m: &Module) -> CModule {
    // Build the list of modules to link.
    // These are specified using the #link macros.
    let mut link: Vec<String> = Vec::new();
    for node in &m.elements {
        match node {
            ModuleObject::CompatMacro(x) => {
                if x.name == "link" {
                    link.push(x.value.clone())
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
            type_name: type_name.clone(),
            form: form.clone(),
        }],
        ModuleObject::FuncTypedef(FuncTypedef {
            is_pub,
            return_type,
            name,
            params,
        }) => vec![CModuleObject::FuncTypedef {
            is_pub: *is_pub,
            return_type: return_type.clone(),
            name: name.clone(),
            params: params.clone(),
        }],
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
            let mut compat_fields: Vec<CompatStructEntry> = Vec::new();
            for entry in fields {
                match entry {
                    // One fieldlist is multiple fields of the same type.
                    StructEntry::Plain(x) => {
                        for f in &x.forms {
                            compat_fields.push(CompatStructEntry::CompatStructField {
                                type_name: x.type_name.clone(),
                                form: f.clone(),
                            });
                        }
                    }
                    StructEntry::Union(x) => {
                        compat_fields.push(CompatStructEntry::Union(x.clone()));
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
                    type_name: Typename {
                        is_const: false,
                        name: format!("struct {}", struct_name.clone()),
                    },
                    form: TypedefForm {
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
        ModuleObject::Enum(Enum { is_pub, members }) => {
            return vec![CModuleObject::EnumDefinition {
                members: members.clone(),
                is_hidden: !is_pub,
            }];
        }
        ModuleObject::CompatMacro(x) => {
            if x.name == "type" || x.name == "link" {
                return vec![];
            } else {
                return vec![CModuleObject::Macro {
                    name: x.name.clone(),
                    value: x.value.clone(),
                }];
            }
        }
        ModuleObject::ModuleVariable(x) => vec![CModuleObject::ModuleVariable {
            type_name: x.type_name.clone(),
            form: x.form.clone(),
            value: x.value.clone(),
        }],
        // ModuleObject::CompatInclude(x) => vec![CompatModuleObject::CompatInclude(x.clone())],
    }
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
    let mut parameters: Vec<CompatFunctionParameter> = Vec::new();
    for parameter in &node.list {
        for form in &parameter.forms {
            parameters.push(CompatFunctionParameter {
                type_name: parameter.type_name.clone(),
                form: form.clone(),
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
    let func = CompatFunctionDeclaration {
        is_static: !is_pub,
        type_name: typename.clone(),
        form: form.clone(),
        parameters: translate_function_parameters(&parameters),
        body: body.clone(),
    };
    let decl = CModuleObject::FunctionForwardDeclaration {
        is_static: func.is_static,
        type_name: func.type_name.clone(),
        form: func.form.clone(),
        parameters: func.parameters.clone(),
    };
    let mut r = vec![CModuleObject::FunctionDefinition(func)];
    if format::format_form(&form) != "main" {
        r.push(decl);
    }
    return r;
}
