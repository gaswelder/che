use crate::format;
use crate::nodes::*;
use crate::parser;
use std::collections::HashSet;

pub fn translate(m: &Module) -> CompatModule {
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
    let mut elements: Vec<CompatModuleObject> = Vec::new();
    for element in &m.elements {
        for node in translate_module_object(element) {
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
        elements.push(CompatModuleObject::CompatInclude(format!("<{}.h>", n)));
    }

    // Reorder the elements so that typedefs and similar preamble elements
    // come first.
    let mut groups: Vec<Vec<CompatModuleObject>> =
        vec![vec![], vec![], vec![], vec![], vec![], vec![]];
    let mut set: HashSet<String> = HashSet::new();
    for element in elements {
        let order = match element {
            CompatModuleObject::CompatInclude(_) => 0,
            CompatModuleObject::CompatMacro(_) => 0,
            CompatModuleObject::CompatStructForwardDeclaration(_) => 1,
            CompatModuleObject::Typedef { .. } => 2,
            CompatModuleObject::CompatStructDefinition(_) => 3,
            CompatModuleObject::Enum { .. } => 3,
            CompatModuleObject::CompatFunctionForwardDeclaration(_) => 4,
            _ => 5,
        };

        match &element {
            CompatModuleObject::Typedef {
                type_name, form, ..
            } => {
                let s = format::format_typedef(&type_name, &form);
                if set.contains(&s) {
                    continue;
                }
                set.insert(s);
            }
            CompatModuleObject::CompatStructDefinition(x) => {
                let s = format::format_compat_struct_definition(&x);
                if set.contains(&s) {
                    continue;
                }
                set.insert(s);
            }
            _ => {}
        }
        groups[order].push(element);
    }
    let mut sorted_elements: Vec<CompatModuleObject> = vec![CompatModuleObject::CompatSplit {
        text: String::from(format!("/* -------{}------- */", &m.id)),
    }];
    for group in groups {
        if group.len() == 0 {
            continue;
        }
        sorted_elements.push(CompatModuleObject::CompatSplit {
            text: String::from("/* -------------- */"),
        });
        for e in group {
            sorted_elements.push(e)
        }
    }

    return CompatModule {
        id: m.id.clone(),
        elements: sorted_elements,
        link,
    };
}

fn translate_module_object(x: &ModuleObject) -> Vec<CompatModuleObject> {
    match x {
        ModuleObject::Typedef {
            is_pub,
            type_name,
            form,
        } => translate_typedef(*is_pub, type_name, form),
        ModuleObject::Import { path } => {
            let module = parser::get_module(&path).unwrap();
            let compat = translate(&module);
            return get_module_synopsis(compat);
        }
        ModuleObject::FunctionDeclaration {
            is_pub,
            type_name,
            form,
            parameters,
            body,
        } => translate_function_declaration(*is_pub, type_name, form, parameters, body),
        ModuleObject::Enum { is_pub, members } => {
            return vec![CompatModuleObject::Enum {
                members: members.clone(),
                is_hidden: !is_pub,
            }];
        }
        ModuleObject::CompatMacro(x) => {
            if x.name == "type" || x.name == "link" {
                return vec![];
            } else {
                return vec![CompatModuleObject::CompatMacro(x.clone())];
            }
        }
        ModuleObject::ModuleVariable(x) => vec![CompatModuleObject::ModuleVariable(x.clone())],
        // ModuleObject::CompatInclude(x) => vec![CompatModuleObject::CompatInclude(x.clone())],
    }
}

fn translate_typedef(
    is_pub: bool,
    type_name: &TypedefTarget,
    form: &TypedefForm,
) -> Vec<CompatModuleObject> {
    match &type_name {
        // A sugary "typedef {int a} foo_t" is translated to
        // "struct __foo_t_struct {int a}; typedef __foo_t_struct foo_t;".
        // And remember that the struct definition itself is sugar that
        // should be translated as well.
        TypedefTarget::AnonymousStruct(s) => {
            let struct_name = format!("__{}_struct", form.alias);

            // Build the compat struct fields.
            let mut fields: Vec<CompatStructEntry> = Vec::new();
            for entry in &s.entries {
                match entry {
                    // One fieldlist is multiple fields of the same type.
                    StructEntry::StructFieldlist(x) => {
                        for f in &x.forms {
                            fields.push(CompatStructEntry::CompatStructField {
                                type_name: x.type_name.clone(),
                                form: f.clone(),
                            });
                        }
                    }
                    StructEntry::Union(x) => {
                        fields.push(CompatStructEntry::Union(x.clone()));
                    }
                }
            }
            vec![
                CompatModuleObject::CompatStructForwardDeclaration(struct_name.clone()),
                CompatModuleObject::CompatStructDefinition(CompatStructDefinition {
                    name: struct_name.clone(),
                    fields,
                    is_pub,
                }),
                CompatModuleObject::Typedef {
                    is_pub,
                    type_name: TypedefTarget::Typename(Typename {
                        is_const: false,
                        name: format!("struct {}", struct_name.clone()),
                    }),
                    form: TypedefForm {
                        stars: "".to_string(),
                        size: 0,
                        params: None,
                        alias: form.alias.clone(),
                    },
                },
            ]
        }
        _ => vec![CompatModuleObject::Typedef {
            is_pub,
            type_name: type_name.clone(),
            form: form.clone(),
        }],
    }
}

fn compat_function_forward_declaration(node: &CompatFunctionDeclaration) -> CompatModuleObject {
    return CompatModuleObject::CompatFunctionForwardDeclaration(
        CompatFunctionForwardDeclaration {
            is_static: node.is_static,
            type_name: node.type_name.clone(),
            form: node.form.clone(),
            parameters: node.parameters.clone(),
        },
    );
}

// Module synopsis is what you would usually extract into a header file:
// function prototypes, typedefs, struct declarations.
fn get_module_synopsis(module: CompatModule) -> Vec<CompatModuleObject> {
    let mut elements: Vec<CompatModuleObject> = vec![];

    for element in module.elements {
        match element {
            CompatModuleObject::Typedef { is_pub, .. } => {
                if is_pub {
                    elements.push(element.clone())
                }
            }
            CompatModuleObject::CompatStructDefinition(x) => {
                if x.is_pub {
                    elements.push(CompatModuleObject::CompatStructDefinition(x))
                }
            }
            CompatModuleObject::CompatFunctionForwardDeclaration(x) => {
                if !x.is_static {
                    elements.push(CompatModuleObject::CompatFunctionForwardDeclaration(x))
                }
            }
            CompatModuleObject::CompatMacro(x) => elements.push(CompatModuleObject::CompatMacro(x)),
            CompatModuleObject::Enum { is_hidden, members } => {
                if !is_hidden {
                    elements.push(CompatModuleObject::Enum { is_hidden, members })
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
) -> Vec<CompatModuleObject> {
    let func = CompatFunctionDeclaration {
        is_static: !is_pub,
        type_name: typename.clone(),
        form: form.clone(),
        parameters: translate_function_parameters(&parameters),
        body: body.clone(),
    };
    let decl = compat_function_forward_declaration(&func);
    let mut r = vec![CompatModuleObject::CompatFunctionDeclaration(func)];
    if format::format_form(&form) != "main" {
        r.push(decl);
    }
    return r;
}
