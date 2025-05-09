use crate::buf::Pos;
use crate::c;
use crate::cspec;
use crate::errors::BuildError;
use crate::format_c;
use crate::format_che;
use crate::node_queries::body_returns;
use crate::node_queries::expression_pos;
use crate::nodes;
use crate::preparser::ModuleInfo;
use crate::types;
use crate::types::Type;
use std::collections::HashSet;

struct Typed<T> {
    typ: types::Type,
    val: T,
}

static DEBUG_TYPES: bool = false;

// Module synopsis is what you would extract into a header file:
// function prototypes, typedefs, struct declarations.
pub fn get_module_synopsis(module: &c::CModule) -> Vec<c::ModElem> {
    let mut elements: Vec<c::ModElem> = vec![];

    for element in &module.elements {
        match element {
            c::ModElem::Typedef(x) => {
                if x.is_pub {
                    elements.push(c::ModElem::Typedef(x.clone()))
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

#[derive(Debug)]
pub struct Binding {
    pos: Pos,
    name: String,
    typ: types::Type,
    used: bool,
    ispub: bool,
}

pub struct TrParams {
    pub this_mod_head: ModuleInfo,
    pub all_mod_heads: Vec<ModuleInfo>,
    pub cmods: Vec<c::CModule>,
    pub mods: Vec<nodes::Module>,
}

struct TrCtx {
    this_mod_head: ModuleInfo,
    all_mod_heads: Vec<ModuleInfo>,
    cmods: Vec<c::CModule>,
    mods: Vec<nodes::Module>,
    scopes: Vec<Vec<Binding>>,
    used_ns: HashSet<String>,
}

fn begin_scope(ctx: &mut TrCtx) {
    ctx.scopes.push(Vec::new());
}

fn end_scope(ctx: &mut TrCtx) -> Result<(), BuildError> {
    // for s in &ctx.scopes {
    //     println!("---{} ---", ctx.this_mod_head.filepath);
    //     for b in s {
    //         println!("- {} {}", b.name, b.used);
    //     }
    // }
    let s = ctx.scopes.pop().unwrap();
    for b in s {
        if !b.ispub && !b.used {
            return Err(BuildError {
                path: ctx.this_mod_head.filepath.clone(),
                pos: b.pos.fmt(),
                message: format!("{} is unused", b.name),
            });
        }
    }
    Ok(())
}

fn find_binding<'a>(ctx: &'a TrCtx, name: &str) -> Option<&'a Binding> {
    ctx.scopes
        .iter()
        .rev()
        .find_map(|s| s.iter().find(|b| b.name == name))
}

fn mark_binding_use(ctx: &mut TrCtx, name: &str) -> bool {
    let mut last_match = None;
    for (s, scope) in ctx.scopes.iter().enumerate() {
        for (b, binding) in scope.iter().enumerate() {
            if binding.name == name {
                last_match = Some((s, b));
            }
        }
    }
    match last_match {
        Some((s, b)) => {
            ctx.scopes[s][b].used = true;
            return true;
        }
        None => {
            return cspec::CTYPES.iter().any(|x| *x == name)
                || cspec::CCONST.iter().any(|x| *x == name)
                || cspec::CFUNCS.iter().any(|x| *x == name)
                || name == "nelem";
        }
    };
}

fn add_binding(ctx: &mut TrCtx, name: &str, pos: Pos, ispub: bool, t: Type) {
    let n = ctx.scopes.len();
    if ctx.scopes[n - 1].iter().any(|x| x.name == name) {
        panic!("{} was already defined at ???", name);
    }
    ctx.scopes[n - 1].push(Binding {
        name: String::from(name),
        pos,
        used: false,
        ispub,
        typ: t,
    });
}

// Translates a module to c module.
pub fn translate(m: &nodes::Module, params: &TrParams) -> Result<c::CModule, BuildError> {
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

    let mut ctx = TrCtx {
        all_mod_heads: params.all_mod_heads.clone(),
        cmods: params.cmods.clone(),
        mods: params.mods.clone(),
        scopes: Vec::new(),
        this_mod_head: params.this_mod_head.clone(),
        used_ns: HashSet::new(),
    };
    begin_scope(&mut ctx);

    // Add module-level identifiers to the scope: enums, modvars, functions.
    // We do that beforehand so there is no need to worry about writing C
    // in compiler-friendly order.
    // Note that in function scopes we do follow the ordering.
    for x in &m.elements {
        match x {
            nodes::ModElem::Macro(x) => {
                if x.name == "type" {
                    add_binding(
                        &mut ctx,
                        &x.value.trim(),
                        x.pos.clone(),
                        false,
                        types::todo(),
                    );
                } else if x.name == "define" {
                    let def = x.value.clone();
                    let mut parts = def.trim().split_whitespace();
                    let id = parts.next().unwrap();
                    // let val = parts.next().unwrap();
                    add_binding(&mut ctx, &id, x.pos.clone(), false, types::todo());
                }
            }
            nodes::ModElem::Enum(x) => {
                for e in &x.entries {
                    add_binding(
                        &mut ctx,
                        &e.name,
                        x.pos.clone(),
                        x.is_pub,
                        types::justval(types::Valtype::Number),
                    );
                }
            }
            nodes::ModElem::StructAlias(x) => {
                add_binding(&mut ctx, &x.typename, x.pos.clone(), x.ispub, types::todo());
            }
            nodes::ModElem::Typedef(x) => {
                add_binding(&mut ctx, &x.alias, x.pos.clone(), x.is_pub, types::todo());
            }
            nodes::ModElem::StructTypedef(x) => {
                add_binding(
                    &mut ctx,
                    &x.name,
                    x.pos.clone(),
                    x.ispub,
                    types::fromstruct(&x),
                );
            }
            nodes::ModElem::ModVar(x) => {
                add_binding(
                    &mut ctx,
                    &x.form.name,
                    x.pos.clone(),
                    false,
                    types::fromtn(&x.typename),
                );
            }
            nodes::ModElem::FuncDecl(x) => {
                let ispub = x.ispub || x.form.name == "main";
                add_binding(
                    &mut ctx,
                    &x.form.name,
                    x.pos.clone(),
                    ispub,
                    types::fromtn(&x.typename),
                );
            }
        }
    }

    let mut elements: Vec<c::ModElem> = Vec::new();

    // Enable POSIX.
    elements.push(c::ModElem::Macro(c::Macro {
        name: "define".to_string(),
        value: "_XOPEN_SOURCE 700".to_string(),
    }));

    // Include all standard C library.
    for n in cspec::CLIBS {
        elements.push(c::ModElem::Include(format!("<{}.h>", n)));
    }
    // Include custom utils.
    elements.push(c::ModElem::Macro(c::Macro {
        name: "define".to_string(),
        value: "nelem(x) (sizeof (x)/sizeof (x)[0])".to_string(),
    }));

    // Inject headers corresponding to the imports.
    for x in expand_imports(&ctx) {
        elements.push(x);
    }

    // Translate module nodes.
    for element in &m.elements {
        for node in tr_mod_elem(element, &mut ctx)? {
            elements.push(node)
        }
    }

    end_scope(&mut ctx)?;

    for imp in &ctx.this_mod_head.imports {
        if !ctx.used_ns.contains(&imp.ns) {
            return Err(BuildError {
                path: ctx.this_mod_head.filepath.clone(),
                pos: format!("1:1"),
                message: format!("unused import: {}", imp.ns),
            });
        }
    }

    Ok(c::CModule {
        elements: reorder_elems(elements),
        link,
    })
}

fn reorder_elems(elements: Vec<c::ModElem>) -> Vec<c::ModElem> {
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
            c::ModElem::Typedef { .. } => 2,
            c::ModElem::DefStruct { .. } => 3,
            c::ModElem::DefEnum { .. } => 3,
            c::ModElem::ForwardFunc { .. } => 4,
            c::ModElem::DeclVar { .. } => 5,
            _ => 6,
        };

        match &element {
            c::ModElem::Typedef(x) => {
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
    sorted_elements
}

fn get_obj_key(obj: &c::ModElem) -> String {
    match &obj {
        c::ModElem::DefEnum(x) => {
            let mut names: Vec<String> = Vec::new();
            for m in x.entries.clone() {
                names.push(m.id.clone());
            }
            format!("enum-{}", names.join(","))
        }
        c::ModElem::Typedef(x) => x.form.alias.clone(),
        c::ModElem::DefStruct(x) => x.name.clone(),
        c::ModElem::Include(x) => x.clone(),
        c::ModElem::Macro(x) => x.value.clone(),
        c::ModElem::ForwardStruct(x) => x.clone(),
        c::ModElem::ForwardFunc(x) => x.form.name.clone(),

        // We shouldn't see these here, they are unexportable.
        c::ModElem::DeclVar(_) => panic!("unexpected item in synopsis"),
        c::ModElem::FuncDef(_) => panic!("unexpected item in synopsis"),
    }
}

fn expand_imports(ctx: &TrCtx) -> Vec<c::ModElem> {
    let mut elements = Vec::new();

    // When the same module is transiently included multiple times, we
    // get duplicate definitions. We filter them out here.
    let mut present = HashSet::new();

    for imp in &ctx.this_mod_head.imports {
        // This relies on the modules being parsed in the order where we are
        // guaranteed to have all current module's dependencies already parsed.
        let pos = ctx
            .all_mod_heads
            .iter()
            .position(|x| x.filepath == imp.path)
            .unwrap();
        let cmodule = &ctx.cmods[pos];
        for obj in get_module_synopsis(cmodule) {
            let id = get_obj_key(&obj);
            if present.contains(&id) {
                continue;
            }
            present.insert(id);
            elements.push(obj);
        }
    }

    elements
}

fn tr_mod_elem(element: &nodes::ModElem, ctx: &mut TrCtx) -> Result<Vec<c::ModElem>, BuildError> {
    match element {
        nodes::ModElem::Typedef(x) => tr_typedef(&x, ctx),
        nodes::ModElem::StructAlias(x) => Ok(tr_struct_alias(&x, ctx)),
        nodes::ModElem::StructTypedef(x) => tr_struct_typedef(&x, ctx),
        nodes::ModElem::FuncDecl(x) => tr_func_decl(&x, ctx),
        nodes::ModElem::Enum(x) => tr_enum(&x, ctx),
        nodes::ModElem::Macro(x) => Ok(tr_macro(&x)),
        nodes::ModElem::ModVar(x) => tr_mod_var(&x, ctx),
    }
}

fn tr_expr(e: &nodes::Expr, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    return match e {
        nodes::Expr::Literal(x) => {
            let typ = match x {
                nodes::Literal::Char(_) => types::justval(types::Valtype::Char),
                nodes::Literal::String(_) => types::constcharp(),
                nodes::Literal::Number(_) => types::justval(types::Valtype::Number),
                nodes::Literal::Null => types::justval(types::Valtype::Null),
            };
            Ok(Typed {
                typ,
                val: c::Expr::Literal(tr_literal(x)),
            })
        }
        nodes::Expr::NsName(x) => tr_nsid_in_expr(x, ctx).map(|x| Typed {
            typ: x.typ,
            val: c::Expr::Ident(x.val),
        }),
        nodes::Expr::ArrIndex(x) => tr_arr_index(&x, ctx),
        nodes::Expr::BinaryOp(x) => tr_binary_op(&x, ctx),
        nodes::Expr::FieldAccess(x) => tr_field_access(&x, ctx),
        nodes::Expr::CompositeLiteral(x) => tr_comp_literal(&x, ctx),
        nodes::Expr::PrefixOperator(x) => tr_prefop(&x, ctx),
        nodes::Expr::PostfixOperator(x) => tr_postop(&x, ctx),
        nodes::Expr::Cast(x) => tr_cast(&x, ctx),
        nodes::Expr::Call(x) => tr_call(&x, ctx),
        nodes::Expr::Sizeof(x) => tr_sizeof(&x, ctx),
    };
}

fn tr_expr0(e: &nodes::Expr, ctx: &mut TrCtx) -> Result<c::Expr, BuildError> {
    tr_expr(e, ctx).map(|x| x.val)
}

fn tr_body(b: &nodes::Body, ctx: &mut TrCtx) -> Result<c::CBody, BuildError> {
    let mut statements: Vec<c::Statement> = Vec::new();
    begin_scope(ctx);
    for s in &b.statements {
        statements.push(match s {
            nodes::Statement::Break => c::Statement::Break,
            nodes::Statement::Continue => c::Statement::Continue,
            nodes::Statement::Expression(x) => {
                match x {
                    nodes::Expr::Call(call) => {
                        // panic is unwrapped at this level because a panic
                        // call is a stand-alone statement and can't be part
                        // of expression.
                        if nodes::is_ident(&call.func, "panic") {
                            let pos = expression_pos(x).fmt();
                            mk_panic(ctx, &pos, &call.args)?
                        } else {
                            c::Statement::Expression(tr_expr0(&x, ctx)?)
                        }
                    }
                    _ => c::Statement::Expression(tr_expr0(&x, ctx)?),
                }
            }
            nodes::Statement::For(x) => tr_for(x, ctx)?,
            nodes::Statement::If(x) => tr_if(x, ctx)?,
            nodes::Statement::Return(x) => tr_return(x, ctx)?,
            nodes::Statement::Switch(x) => tr_switch(x, ctx)?,
            nodes::Statement::VarDecl(x) => tr_vardecl(x, ctx)?,
            nodes::Statement::While(x) => tr_while(x, ctx)?,
        });
    }
    end_scope(ctx)?;
    Ok(c::CBody { statements })
}

fn tr_struct_alias(x: &nodes::StructAlias, ctx: &TrCtx) -> Vec<c::ModElem> {
    let alias = if x.ispub {
        nsprefix(&ctx.this_mod_head.uniqid, &x.typename)
    } else {
        x.typename.clone()
    };
    vec![c::ModElem::Typedef(c::Typedef {
        is_pub: x.ispub,
        typename: c::Typename {
            is_const: false,
            name: format!("struct {}", x.structname),
        },
        form: c::TypedefForm {
            stars: String::new(),
            params: None,
            size: 0,
            alias,
        },
    })]
}

fn tr_typedef(x: &nodes::Typedef, ctx: &mut TrCtx) -> Result<Vec<c::ModElem>, BuildError> {
    let array_size = &x.array_size;
    let function_parameters = &x.function_parameters;
    let dereference_count = &x.dereference_count;
    let type_name = &x.type_name;

    let params = match function_parameters {
        Some(x) => {
            let mut forms: Vec<c::CAnonymousTypeform> = Vec::new();
            for f in &x.forms {
                forms.push(translate_anonymous_typeform(&f, ctx)?);
            }
            Some(c::CAnonymousParameters {
                ellipsis: x.ellipsis,
                forms,
            })
        }
        None => None,
    };

    let b = find_binding(ctx, &x.alias);
    let alias = if b.is_some() && b.unwrap().ispub {
        nsprefix(&ctx.this_mod_head.uniqid, &x.alias)
    } else {
        x.alias.clone()
    };

    Ok(vec![c::ModElem::Typedef(c::Typedef {
        is_pub: x.is_pub,
        typename: tr_typename(type_name, ctx)?,
        form: c::TypedefForm {
            stars: "*".repeat(*dereference_count),
            params,
            size: *array_size,
            alias,
        },
    })])
}

// typedef { int a } foo_t;
fn tr_struct_typedef(
    x: &nodes::StructTypedef,
    ctx: &mut TrCtx,
) -> Result<Vec<c::ModElem>, BuildError> {
    // Translate to struct + typedef struct.

    // If the typedef is private, keep the name as foo_t.
    // If public, use <ns>__foo_t.
    let name = if x.ispub {
        nsprefix(&ctx.this_mod_head.uniqid, &x.name)
    } else {
        x.name.clone()
    };

    let struct_name = format!("__{}_struct", name);

    // Build the compat struct fields.
    let mut fields: Vec<c::CStructItem> = Vec::new();
    for entry in &x.entries {
        match entry {
            // One fieldlist is multiple fields of the same type.
            nodes::StructEntry::Plain(x) => {
                for f in &x.forms {
                    fields.push(c::CStructItem::Field(c::CTypeForm {
                        type_name: tr_typename(&x.typename, ctx)?,
                        form: tr_form(f, ctx, false)?,
                    }));
                }
            }
            nodes::StructEntry::Union(x) => {
                fields.push(c::CStructItem::Union(tr_union(x, ctx)?));
            }
        }
    }
    Ok(vec![
        c::ModElem::ForwardStruct(struct_name.clone()),
        c::ModElem::DefStruct(c::StructDef {
            name: struct_name.clone(),
            fields,
            is_pub: x.ispub,
        }),
        c::ModElem::Typedef(c::Typedef {
            is_pub: x.ispub,
            typename: c::Typename {
                is_const: false,
                name: format!("struct {}", struct_name.clone()),
            },
            form: c::TypedefForm {
                stars: "".to_string(),
                size: 0,
                params: None,
                alias: String::from(name),
            },
        }),
    ])
}

// enum { A=1, B, C }
fn tr_enum(x: &nodes::Enum, ctx: &mut TrCtx) -> Result<Vec<c::ModElem>, BuildError> {
    let mut entries = Vec::new();
    for e in &x.entries {
        let id = if x.is_pub {
            nsprefix(&ctx.this_mod_head.uniqid, &e.name)
        } else {
            e.name.clone()
        };
        entries.push(c::EnumEntry {
            id,
            value: match &e.val {
                Some(v) => Some(tr_expr0(v, ctx)?),
                None => None,
            },
        })
    }
    Ok(vec![c::ModElem::DefEnum(c::EnumDef {
        entries,
        is_hidden: !x.is_pub,
    })])
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

// int foo = 12;
fn tr_mod_var(x: &nodes::VarDecl, ctx: &mut TrCtx) -> Result<Vec<c::ModElem>, BuildError> {
    Ok(vec![c::ModElem::DeclVar(c::DeclVar {
        type_name: tr_typename(&x.typename, ctx)?,
        form: tr_form(&x.form, ctx, true)?,
        value: tr_expr0(x.value.as_ref().unwrap(), ctx)?,
    })])
}

// *foo[]
fn tr_form(x: &nodes::Form, ctx: &mut TrCtx, publishable: bool) -> Result<c::Form, BuildError> {
    let mut indexes = Vec::new();
    for index in &x.indexes {
        indexes.push(match index {
            Some(e) => Some(tr_expr0(e, ctx)?),
            None => None,
        })
    }

    let b = find_binding(ctx, &x.name);
    let name = if publishable && b.is_some() && b.unwrap().ispub && x.name != "main" {
        nsprefix(&ctx.this_mod_head.uniqid, &x.name)
    } else {
        x.name.clone()
    };

    Ok(c::Form {
        indexes,
        name,
        stars: "*".repeat(x.hops),
    })
}

fn tr_union(x: &nodes::Union, ctx: &mut TrCtx) -> Result<c::CUnion, BuildError> {
    let form = tr_form(&x.form, ctx, false)?;
    let mut fields = Vec::new();
    for f in &x.fields {
        fields.push(c::CUnionField {
            type_name: tr_typename(&f.type_name, ctx)?,
            form: tr_form(&f.form, ctx, false)?,
        });
    }
    Ok(c::CUnion { form, fields })
}

// <..> <op> <..>
fn tr_binary_op(x: &nodes::BinaryOp, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    let a = tr_expr(&x.a, ctx)?;
    let b = tr_expr(&x.b, ctx)?;
    Ok(Typed {
        typ: types::todo(),
        val: c::Expr::BinaryOp(c::BinaryOp {
            op: x.op.clone(),
            a: Box::new(a.val),
            b: Box::new(b.val),
        }),
    })
}

// <op> <..>
fn tr_prefop(x: &nodes::PrefixOp, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    let operand = tr_expr(&x.operand, ctx)?;
    let typ = match x.operator.as_str() {
        "++" | "--" => operand.typ,
        "!" | "-" | "~" => operand.typ,
        // "*" => match types::deref(t) {
        //     Ok(r) => r,
        //     Err(err) => {
        //         state.type_errors.push(Error {
        //             message: err,
        //             pos: Pos { col: 0, line: 0 },
        //         });
        //         return Type::Unknown;
        //     }
        // },
        // "&" => types::addr(t),
        _ => {
            types::todo()
            // dbg!(operator);
            // todo!();
        }
    };
    Ok(Typed {
        typ,
        val: c::Expr::PrefixOp {
            operator: x.operator.clone(),
            operand: Box::new(operand.val),
        },
    })
}

// <..> <op>
fn tr_postop(x: &nodes::PostfixOp, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    let operand = tr_expr(&x.operand, ctx)?;
    let typ = match x.operator.as_str() {
        "++" | "--" => operand.typ,
        _ => {
            dbg!(x);
            todo!();
        }
    };
    Ok(Typed {
        typ,
        val: c::Expr::PostfixOp {
            operator: x.operator.clone(),
            operand: Box::new(operand.val),
        },
    })
}

// (typename) <...>
fn tr_cast(x: &nodes::Cast, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    let operand = tr_expr(&x.operand, ctx)?;
    Ok(Typed {
        typ: types::todo(),
        val: c::Expr::Cast {
            type_name: translate_anonymous_typeform(&x.type_name, ctx)?,
            operand: Box::new(operand.val),
        },
    })
}

// <..> [...]
fn tr_arr_index(x: &nodes::ArrayIndex, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    let arr = tr_expr(&x.array, ctx)?;
    let ind = tr_expr(&x.index, ctx)?;
    Ok(Typed {
        typ: types::todo(),
        val: c::Expr::ArrayIndex {
            array: Box::new(arr.val),
            index: Box::new(ind.val),
        },
    })
}

// <..> -> field
// <..> . field
fn tr_field_access(x: &nodes::FieldAccess, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    let target = tr_expr(&x.target, ctx)?;
    if DEBUG_TYPES {
        println!(
            "target={}: {}",
            format_che::fmt_expr(&x.target),
            target.typ.fmt()
        );
    }
    Ok(Typed {
        typ: types::todo(),
        val: c::Expr::FieldAccess {
            op: x.op.clone(),
            target: Box::new(target.val),
            field_name: x.field_name.clone(),
        },
    })
}

// { .field = <...>, .field = <...>, ... }
fn tr_comp_literal(x: &nodes::CompLiteral, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    let mut entries = Vec::new();
    for e in &x.entries {
        let key = match &e.key {
            Some(x) => match x {
                nodes::Expr::NsName(ns_name) => {
                    if ns_name.ns != "" {
                        panic!("expected field name")
                    }
                    Some(c::Expr::Ident(ns_name.name.clone()))
                }
                _ => {
                    dbg!(x);
                    panic!("unexpected entry");
                }
            },
            None => None,
        };
        let val = tr_expr(&e.value, ctx)?;
        entries.push(c::CCompositeLiteralEntry {
            is_index: e.is_index,
            key,
            val: val.val,
        })
    }
    Ok(Typed {
        typ: types::todo(),
        val: c::Expr::CompositeLiteral(c::CCompositeLiteral { entries }),
    })
}

// sizeof(...)
fn tr_sizeof(x: &nodes::Sizeof, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    let argument = &x.argument;
    let c = *argument.clone();
    let arg = match c {
        nodes::SizeofArg::Expr(x) => c::SizeofArg::Expression(tr_expr0(&x, ctx)?),
        nodes::SizeofArg::Typename(x) => c::SizeofArg::Typename(tr_typename(&x, ctx)?),
    };
    Ok(Typed {
        typ: types::todo(),
        val: c::Expr::Sizeof { arg: Box::new(arg) },
    })
}

// f(args)
fn tr_call(x: &nodes::Call, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    let mut args: Vec<c::Expr> = Vec::new();
    for a in &x.args {
        let e = tr_expr(a, ctx)?;
        if DEBUG_TYPES {
            println!("{}: {}", format_che::fmt_expr(&a), e.typ.fmt());
        }
        args.push(e.val)
    }
    let func = tr_expr(&x.func, ctx)?;
    if DEBUG_TYPES {
        println!("{}: {}", format_che::fmt_expr(&x.func), func.typ.fmt());
    }
    let typ = match func.typ.val {
        types::Valtype::Func { rettype } => *rettype,
        _ => types::todo(),
    };

    Ok(Typed {
        typ,
        val: c::Expr::Call {
            func: Box::new(func.val),
            args,
        },
    })
}

fn translate_anonymous_typeform(
    x: &nodes::AnonymousTypeform,
    ctx: &mut TrCtx,
) -> Result<c::CAnonymousTypeform, BuildError> {
    Ok(c::CAnonymousTypeform {
        type_name: tr_typename(&x.type_name, ctx)?,
        ops: x.ops.clone(),
    })
}

fn tr_func_params(x: &nodes::FuncParams, ctx: &mut TrCtx) -> Result<c::FuncParams, BuildError> {
    // One parameter, like in structs, is one type and multiple entries,
    // while the C parameters are always one type and one entry.
    let mut parameters: Vec<c::CTypeForm> = Vec::new();
    for param in &x.list {
        for form in &param.forms {
            parameters.push(c::CTypeForm {
                type_name: tr_typename(&param.typename, ctx)?,
                form: tr_form(form, ctx, false)?,
            })
        }
    }
    Ok(c::FuncParams {
        list: parameters,
        variadic: x.variadic,
    })
}

// pub? int f(...) {...}
fn tr_func_decl(x: &nodes::FuncDecl, ctx: &mut TrCtx) -> Result<Vec<c::ModElem>, BuildError> {
    begin_scope(ctx);

    for p in &x.params.list {
        for f in &p.forms {
            add_binding(
                ctx,
                &f.name,
                f.pos.clone(),
                false,
                types::fromtn(&p.typename),
            );
        }
    }

    let tbody = tr_body(&x.body, ctx)?;
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
    let mut r = vec![c::ModElem::FuncDef(c::FuncDef {
        is_static: !x.ispub,
        type_name: tr_typename(&x.typename, ctx)?,
        form: tr_form(&x.form, ctx, true)?,
        parameters: tr_func_params(&x.params, ctx)?,
        body: rbody,
    })];
    if x.form.name != "main" {
        r.push(c::ModElem::ForwardFunc(c::ForwardFunc {
            is_static: !x.ispub,
            type_name: tr_typename(&x.typename, ctx)?,
            form: tr_form(&x.form, ctx, true)?,
            parameters: tr_func_params(&x.params, ctx)?,
        }));
    }
    end_scope(ctx)?;

    if (!nodes::is_void(&x.typename) || x.form.hops == 1) && !body_returns(&x.body) {
        return Err(BuildError {
            message: format!("{}: missing return", x.form.name),
            pos: x.pos.fmt(),
            path: ctx.this_mod_head.filepath.clone(),
        });
    }

    Ok(r)
}

// mod.foo_t
fn tr_typename(t: &nodes::Typename, ctx: &mut TrCtx) -> Result<c::Typename, BuildError> {
    Ok(c::Typename {
        is_const: t.is_const,
        name: tr_nsid(&t.name, ctx)?,
    })
}

fn tr_nsid_in_expr(x: &nodes::NsName, ctx: &mut TrCtx) -> Result<Typed<String>, BuildError> {
    let typ: Type;
    if x.ns == "" {
        match find_binding(ctx, &x.name) {
            Some(b) => {
                typ = b.typ.clone();
            }
            None => match types::stdlib(&x.name) {
                Some(b) => {
                    typ = b;
                }
                None => {
                    typ = types::todo();
                }
            },
        }
    } else {
        typ = types::todo();
    }
    let val = tr_nsid(x, ctx)?;
    Ok(Typed { typ, val })
}

// foo.bar() -> _foo_123__bar()
fn tr_nsid(nsid: &nodes::NsName, ctx: &mut TrCtx) -> Result<String, BuildError> {
    // "OS" is a special namespace that is simply omitted.
    if nsid.ns == "OS" {
        return Ok(nsid.name.clone());
    }
    if nsid.ns != "" {
        ctx.used_ns.insert(nsid.ns.clone());
        // ns -> path
        // Get the module imported as this namespace.
        let pos = ctx
            .this_mod_head
            .imports
            .iter()
            .position(|x| x.ns == nsid.ns)
            .unwrap();
        let path = &ctx.this_mod_head.imports[pos].path;

        // path -> pos
        let pos = ctx
            .all_mod_heads
            .iter()
            .position(|x| x.filepath == *path)
            .unwrap();

        let id = &ctx.all_mod_heads[pos].uniqid;

        let exports = &ctx.mods[pos].exports;

        if !nodes::exports_has(&exports, &nsid.name) {
            return Err(BuildError {
                message: format!("{} doesn't have exported {}", &nsid.ns, &nsid.name),
                pos: nsid.pos.fmt(),
                path: ctx.this_mod_head.filepath.clone(),
            });
        }
        return Ok(nsprefix(id, &nsid.name));
    }
    let b = find_binding(ctx, &nsid.name);
    let name = if b.is_some() && b.unwrap().ispub {
        nsprefix(&ctx.this_mod_head.uniqid, &nsid.name)
    } else {
        nsid.name.clone()
    };
    if !mark_binding_use(ctx, &nsid.name) {
        return Err(BuildError {
            message: format!("{} is undefined", &nsid.name),
            pos: nsid.pos.fmt(),
            path: ctx.this_mod_head.filepath.clone(),
        });
    }
    return Ok(name);
}

fn nsprefix(prefix: &str, id: &str) -> String {
    format!("{}__{}", prefix, id)
}

// for (...) { ... }
fn tr_for(x: &nodes::For, ctx: &mut TrCtx) -> Result<c::Statement, BuildError> {
    begin_scope(ctx);
    let r = c::Statement::For {
        init: match &x.init {
            Some(init) => Some(match &init {
                // i = 0
                nodes::ForInit::Expr(x) => c::ForInit::Expr(tr_expr0(x, ctx)?),

                // int i = 0
                nodes::ForInit::DeclLoopCounter {
                    type_name,
                    form,
                    value,
                } => {
                    add_binding(
                        ctx,
                        &form.name,
                        form.pos.clone(),
                        false,
                        types::fromtn(&type_name),
                    );
                    c::ForInit::DeclLoopCounter(c::DeclVar {
                        type_name: tr_typename(type_name, ctx)?,
                        form: tr_form(form, ctx, false)?,
                        value: tr_expr0(value, ctx)?,
                    })
                }
            }),
            None => None,
        },
        condition: match &x.condition {
            Some(x) => Some(tr_expr0(&x, ctx)?),
            None => None,
        },
        action: match &x.action {
            Some(x) => Some(tr_expr0(&x, ctx)?),
            None => None,
        },
        body: tr_body(&x.body, ctx)?,
    };
    end_scope(ctx)?;
    Ok(r)
}

// if (...) { ... }
// if (...) { ... } else { ... }
fn tr_if(x: &nodes::If, ctx: &mut TrCtx) -> Result<c::Statement, BuildError> {
    let else_body = match &x.else_body {
        Some(body) => Some(tr_body(&body, ctx)?),
        None => None,
    };
    Ok(c::Statement::If {
        condition: tr_expr0(&x.condition, ctx)?,
        body: tr_body(&x.body, ctx)?,
        else_body,
    })
}

// return
// return <...>
fn tr_return(x: &nodes::Return, ctx: &mut TrCtx) -> Result<c::Statement, BuildError> {
    Ok(c::Statement::Return {
        expression: match &x.expression {
            Some(e) => Some(tr_expr0(e, ctx)?),
            None => None,
        },
    })
}

// switch (...) { case ... default ... }
// switch str (...) { case ... default ... }
fn tr_switch(x: &nodes::Switch, ctx: &mut TrCtx) -> Result<c::Statement, BuildError> {
    if x.is_str {
        mk_str_switch(x, ctx)
    } else {
        mk_old_switch(x, ctx)
    }
}

fn mk_old_switch(x: &nodes::Switch, ctx: &mut TrCtx) -> Result<c::Statement, BuildError> {
    let value = &x.value;
    let cases = &x.cases;
    let default = &x.default_case;

    let mut tcases: Vec<c::CSwitchCase> = Vec::new();
    for c in cases {
        let mut values = Vec::new();
        for v in &c.values {
            let tv = match v {
                nodes::SwitchCaseValue::Ident(x) => {
                    c::CSwitchCaseValue::Ident(tr_nsid_in_expr(x, ctx)?.val)
                }
                nodes::SwitchCaseValue::Literal(x) => c::CSwitchCaseValue::Literal(tr_literal(x)),
            };
            values.push(tv)
        }
        tcases.push(c::CSwitchCase {
            values,
            body: tr_body(&c.body, ctx)?,
        })
    }
    Ok(c::Statement::Switch(c::Switch {
        value: tr_expr0(value, ctx)?,
        cases: tcases,
        default: match default {
            Some(x) => Some(tr_body(&x, ctx)?),
            None => None,
        },
    }))
}

fn mk_str_switch(x: &nodes::Switch, ctx: &mut TrCtx) -> Result<c::Statement, BuildError> {
    let value = &x.value;
    let cases = &x.cases;
    let default = &x.default_case;
    let switchval = tr_expr0(value, ctx)?;

    let c0 = &cases[0];
    Ok(c::Statement::If {
        condition: mk_switchstr_cond(ctx, c0, &switchval)?,
        body: tr_body(&c0.body, ctx)?,
        else_body: mk_switchstr_else(cases, 1, default, ctx, &switchval)?,
    })
}

fn mk_switchstr_cond(
    ctx: &mut TrCtx,
    sw: &nodes::SwitchCase,
    switchval: &c::Expr,
) -> Result<c::Expr, BuildError> {
    let mut args: Vec<c::Expr> = Vec::new();
    args.push(match &sw.values[0] {
        nodes::SwitchCaseValue::Ident(ns_name) => {
            c::Expr::Ident(tr_nsid_in_expr(&ns_name, ctx)?.val)
        }
        nodes::SwitchCaseValue::Literal(literal) => c::Expr::Literal(tr_literal(&literal)),
    });
    args.push(switchval.clone());
    let mut root = mk_neg(mk_call0("strcmp", args));
    let n = sw.values.len();
    for i in 1..n {
        let mut args: Vec<c::Expr> = Vec::new();
        args.push(match &sw.values[i] {
            nodes::SwitchCaseValue::Ident(ns_name) => {
                c::Expr::Ident(tr_nsid_in_expr(&ns_name, ctx)?.val)
            }
            nodes::SwitchCaseValue::Literal(literal) => c::Expr::Literal(tr_literal(&literal)),
        });
        args.push(switchval.clone());
        root = mk_or(root, mk_neg(mk_call0("strcmp", args)));
    }
    Ok(root)
}

fn mk_switchstr_else(
    cases: &Vec<nodes::SwitchCase>,
    i: usize,
    default: &Option<nodes::Body>,
    ctx: &mut TrCtx,
    switchval: &c::Expr,
) -> Result<Option<c::CBody>, BuildError> {
    if i == cases.len() {
        return Ok(match default {
            Some(x) => Some(tr_body(x, ctx)?),
            None => None,
        });
    }
    let c = &cases[i];
    let e = c::Statement::If {
        condition: mk_switchstr_cond(ctx, c, &switchval)?,
        body: tr_body(&c.body, ctx)?,
        else_body: mk_switchstr_else(cases, i + 1, default, ctx, switchval)?,
    };
    Ok(Some(c::CBody {
        statements: vec![e],
    }))
}

// int foo;
// int foo = 1;
fn tr_vardecl(x: &nodes::VarDecl, ctx: &mut TrCtx) -> Result<c::Statement, BuildError> {
    add_binding(
        ctx,
        &x.form.name,
        x.form.pos.clone(),
        false,
        types::fromtn(&x.typename),
    );
    Ok(c::Statement::VarDecl {
        type_name: tr_typename(&x.typename, ctx)?,
        forms: vec![tr_form(&x.form, ctx, false)?],
        values: vec![match &x.value {
            Some(x) => Some(tr_expr0(&x, ctx)?),
            None => None,
        }],
    })
}

// while (...) { ... }
fn tr_while(x: &nodes::While, ctx: &mut TrCtx) -> Result<c::Statement, BuildError> {
    Ok(c::Statement::While {
        cond: tr_expr0(&x.cond, ctx)?,
        body: tr_body(&x.body, ctx)?,
    })
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

fn mk_call0(func: &str, args: Vec<c::Expr>) -> c::Expr {
    c::Expr::Call {
        func: Box::new(c::Expr::Ident(String::from(func))),
        args,
    }
}

fn mk_call(func: &str, args: Vec<c::Expr>) -> c::Statement {
    c::Statement::Expression(mk_call0(func, args))
}

fn mk_panic(
    ctx: &mut TrCtx,
    pos: &String,
    arguments: &Vec<nodes::Expr>,
) -> Result<c::Statement, BuildError> {
    let mut args = vec![c::Expr::Ident(String::from("stderr"))];
    for arg in arguments {
        args.push(tr_expr0(arg, ctx)?);
    }
    Ok(c::Statement::Block {
        statements: vec![
            mk_call(
                "fprintf",
                vec![
                    c::Expr::Ident(String::from("stderr")),
                    c::Expr::Literal(c::CLiteral::String(String::from("*** panic at %s ***\\n"))),
                    c::Expr::Literal(c::CLiteral::String(pos.clone())),
                ],
            ),
            mk_call("fprintf", args),
            mk_call(
                "fprintf",
                vec![
                    c::Expr::Ident(String::from("stderr")),
                    c::Expr::Literal(c::CLiteral::String(String::from("\\n"))),
                ],
            ),
            mk_call(
                "exit",
                vec![c::Expr::Literal(c::CLiteral::Number(String::from("1")))],
            ),
        ],
    })
}

// Makes a negation of the given expression.
fn mk_neg(operand: c::Expr) -> c::Expr {
    c::Expr::PrefixOp {
        operator: String::from("!"),
        operand: Box::new(operand),
    }
}

fn mk_or(a: c::Expr, b: c::Expr) -> c::Expr {
    c::Expr::BinaryOp(c::BinaryOp {
        op: String::from("||"),
        a: Box::new(a),
        b: Box::new(b),
    })
}
