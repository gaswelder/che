use crate::format;
use crate::nodes::*;
use crate::parser;
use std::collections::HashSet;

pub fn translate(module: &Module) -> CompatModule {
    // Build the list of modules to link.
    // These are specified using the #link macros.
    let mut link: Vec<String> = Vec::new();
    for node in &module.elements {
        match node {
            ModuleObject::CompatMacro(x) => {
                if x.name == "link" {
                    link.push(x.value.clone())
                }
            }
            _ => {}
        }
    }

    // Translate each node into a C node. Some nodes are valid C nodes
    // already, but some are not and have to be converted to their
    // "Compat..." analogs.
    let mut elements: Vec<CompatModuleObject> = Vec::new();
    for element in &module.elements {
        for node in translate_node(element) {
            elements.push(node)
        }
    }

    // Since we all know what the standard C library is, just include it all
    // everywhere. We could analyze the module contents and add only those
    // headers that are required, but why bother.
    let std = vec![
        "assert", "ctype", "errno", "limits", "math", "stdarg", "stdbool", "stddef", "stdint",
        "stdio", "stdlib", "string", "time",
    ];
    for n in std {
        elements.push(CompatModuleObject::CompatInclude(CompatInclude {
            name: format!("<{}.h>", n),
        }));
    }

    // Reorder the elements so that typedefs and similar preamble elements
    // come first.
    let mut groups: Vec<Vec<CompatModuleObject>> = vec![
        vec![CompatModuleObject::CompatSplit {
            text: String::from("/* -------------- */"),
        }],
        vec![CompatModuleObject::CompatSplit {
            text: String::from("/* -------------- */"),
        }],
        vec![CompatModuleObject::CompatSplit {
            text: String::from("/* -------------- */"),
        }],
        vec![CompatModuleObject::CompatSplit {
            text: String::from("/* -------------- */"),
        }],
        vec![CompatModuleObject::CompatSplit {
            text: String::from("/* -------------- */"),
        }],
        vec![CompatModuleObject::CompatSplit {
            text: String::from("/* -------------- */"),
        }],
    ];
    let mut set: HashSet<String> = HashSet::new();
    for element in elements {
        let order = match element {
            CompatModuleObject::CompatInclude(_) => 0,
            CompatModuleObject::CompatMacro(_) => 0,
            CompatModuleObject::CompatStructForwardDeclaration(_) => 1,
            CompatModuleObject::Typedef(_) => 2,
            CompatModuleObject::CompatStructDefinition(_) => 3,
            CompatModuleObject::CompatEnum(_) => 3,
            CompatModuleObject::CompatFunctionForwardDeclaration(_) => 4,
            _ => 5,
        };

        match &element {
            CompatModuleObject::Typedef(x) => {
                let s = format::format_typedef(&x);
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
        text: String::from(format!("/* -------{}------- */", &module.id)),
    }];
    for group in groups {
        for e in group {
            sorted_elements.push(e)
        }
    }

    return CompatModule {
        id: module.id.clone(),
        elements: sorted_elements,
        link,
    };
}

fn translate_node(node: &ModuleObject) -> Vec<CompatModuleObject> {
    match node {
        ModuleObject::Typedef(x) => translate_typedef(x),
        ModuleObject::Import(x) => {
            let module = parser::get_module(&x.path).unwrap();
            let compat = translate(&module);
            return get_module_synopsis(compat);
        }
        ModuleObject::FunctionDeclaration(x) => translate_function_declaration(x),
        ModuleObject::Enum(x) => {
            return vec![CompatModuleObject::CompatEnum(CompatEnum {
                members: x.members.clone(),
                is_hidden: !x.is_pub,
            })];
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

fn translate_typedef(node: &Typedef) -> Vec<CompatModuleObject> {
    match &node.type_name {
        // A sugary "typedef {int a} foo_t" is translated to
        // "struct __foo_t_struct {int a}; typedef __foo_t_struct foo_t;".
        // And remember that the struct definition itself is sugar that
        // should be translated as well.
        TypedefTarget::AnonymousStruct(s) => {
            let struct_name = format!("__{}_struct", node.form.alias);

            // Build the compat struct fields.
            let mut fields: Vec<CompatStructEntry> = Vec::new();
            for entry in &s.entries {
                match entry {
                    // One fieldlist is multiple fields of the same type.
                    StructEntry::StructFieldlist(x) => {
                        for f in &x.forms {
                            fields.push(CompatStructEntry::CompatStructField(CompatStructField {
                                type_name: x.type_name.clone(),
                                form: f.clone(),
                            }));
                        }
                    }
                    StructEntry::Union(x) => {
                        fields.push(CompatStructEntry::Union(x.clone()));
                    }
                }
            }
            vec![
                CompatModuleObject::CompatStructForwardDeclaration(
                    CompatStructForwardDeclaration {
                        kind: "c_compat_struct_forward_declaration".to_string(),
                        name: struct_name.clone(),
                    },
                ),
                CompatModuleObject::CompatStructDefinition(CompatStructDefinition {
                    kind: "c_compat_struct_definition".to_string(),
                    name: struct_name.clone(),
                    fields: fields,
                }),
                CompatModuleObject::Typedef(Typedef {
                    kind: "c_typedef".to_string(),
                    type_name: TypedefTarget::Type(Type {
                        kind: "c_type".to_string(),
                        is_const: false,
                        type_name: format!("struct {}", struct_name.clone()),
                    }),
                    form: TypedefForm {
                        stars: "".to_string(),
                        size: 0,
                        params: None,
                        alias: node.form.alias.clone(),
                    },
                }),
            ]
        }
        _ => vec![CompatModuleObject::Typedef(Typedef {
            kind: "c_typedef".to_string(),
            type_name: node.type_name.clone(),
            form: node.form.clone(),
        })],
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
            // Write out non-static function prototypes.
            // CompatModuleObject::CompatFunctionDeclaration(x) => {
            //     if !x.is_static {
            //         elements.push(compat_function_forward_declaration(&x));
            //     }
            // }
            CompatModuleObject::Typedef(x) => elements.push(CompatModuleObject::Typedef(x)),
            CompatModuleObject::CompatStructDefinition(x) => {
                elements.push(CompatModuleObject::CompatStructDefinition(x))
            }
            CompatModuleObject::CompatFunctionForwardDeclaration(x) => {
                if !x.is_static {
                    elements.push(CompatModuleObject::CompatFunctionForwardDeclaration(x))
                }
            }
            CompatModuleObject::CompatMacro(x) => elements.push(CompatModuleObject::CompatMacro(x)),
            CompatModuleObject::CompatEnum(x) => {
                if !x.is_hidden {
                    elements.push(CompatModuleObject::CompatEnum(x))
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

fn translate_function_declaration(node: &FunctionDeclaration) -> Vec<CompatModuleObject> {
    let func = CompatFunctionDeclaration {
        kind: "c_compat_function_declaration".to_string(),
        is_static: !node.is_pub,
        type_name: node.type_name.clone(),
        form: node.form.clone(),
        parameters: translate_function_parameters(&node.parameters),
        body: node.body.clone(),
    };
    return vec![
        compat_function_forward_declaration(&func),
        CompatModuleObject::CompatFunctionDeclaration(func),
    ];
}
