use crate::buf::Pos;
use crate::c;
use crate::cspec;
use crate::cspec::CCONST;
use crate::cspec::CTYPES;
use crate::errors::BuildError;
use crate::format_c;
use crate::format_che;
use crate::node_queries::body_returns;
use crate::node_queries::expression_pos;
use crate::nodes;
use crate::nodes::exports_has;
use crate::nodes::is_void;
use crate::nodes::Literal;
use crate::preparser::ModuleInfo;
use crate::types::Type;
use std::collections::HashSet;

struct Typed<T> {
    typ: Type,
    val: T,
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

pub struct Binding {
    name: String,
    pos: Pos,
    used: bool,
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
        if !b.used {
            return Err(BuildError {
                path: ctx.this_mod_head.filepath.clone(),
                pos: b.pos.fmt(),
                message: format!("{} is unused", b.name),
            });
        }
    }
    Ok(())
}

fn mark_binding_use(ctx: &mut TrCtx, name: &str) -> bool {
    // println!("mark {}", name);
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
            // println!("checking {}", name);
            return CTYPES.iter().any(|x| *x == name) || CCONST.iter().any(|x| *x == name);
        }
    };
}

fn add_binding(ctx: &mut TrCtx, name: &str, pos: Pos, ispub: bool) {
    let n = ctx.scopes.len();
    if ctx.scopes[n - 1].iter().any(|x| x.name == name) {
        panic!("{} was already defined at ???", name);
    }
    ctx.scopes[n - 1].push(Binding {
        name: String::from(name),
        pos,
        used: ispub,
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
                    // #type hints tell about global ambient types, so they
                    // are considered "public" and exclude from usage checks.
                    add_binding(&mut ctx, &x.value.trim(), x.pos.clone(), true)
                }
            }
            nodes::ModElem::Enum(x) => {
                for e in &x.entries {
                    add_binding(&mut ctx, &e.id.name, x.pos.clone(), x.is_pub);
                }
            }
            nodes::ModElem::StructAliasTypedef(x) => {
                add_binding(&mut ctx, &x.type_alias, x.pos.clone(), x.is_pub);
            }
            nodes::ModElem::Typedef(x) => {
                add_binding(&mut ctx, &x.alias.name, x.pos.clone(), x.is_pub);
            }
            nodes::ModElem::StructTypedef(x) => {
                add_binding(&mut ctx, &x.name.name, x.pos.clone(), x.is_pub);
            }
            nodes::ModElem::ModuleVariable(x) => {
                add_binding(&mut ctx, &x.form.name, x.pos.clone(), false);
            }
            nodes::ModElem::DeclFunc(x) => {
                let ispub = x.is_pub || x.form.name == "main";
                add_binding(&mut ctx, &x.form.name, x.pos.clone(), ispub);
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
        // imp.
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
        c::ModElem::DefType(x) => x.form.alias.clone(),
        c::ModElem::DefStruct(x) => x.name.clone(),
        c::ModElem::Include(x) => x.clone(),
        c::ModElem::Macro(x) => x.value.clone(),
        c::ModElem::ForwardStruct(x) => x.clone(),
        c::ModElem::ForwardFunc(x) => x.form.name.clone(),

        // We shouldn't see these here, they are unexportable.
        c::ModElem::DeclVar(_) => panic!("unexpected item in synopsis"),
        c::ModElem::DefFunc(_) => panic!("unexpected item in synopsis"),
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
        nodes::ModElem::StructAliasTypedef(x) => Ok(tr_struct_alias(&x)),
        nodes::ModElem::StructTypedef(x) => tr_struct_typedef(&x, ctx),
        nodes::ModElem::DeclFunc(x) => tr_func_decl(&x, ctx),
        nodes::ModElem::Enum(x) => tr_enum(&x, ctx),
        nodes::ModElem::Macro(x) => Ok(tr_macro(&x)),
        nodes::ModElem::ModuleVariable(x) => tr_mod_var(&x, ctx),
    }
}

fn tr_expr(e: &nodes::Expression, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    return match e {
        nodes::Expression::Literal(x) => {
            let typ = match x {
                Literal::Char(_) => Type::Char,
                Literal::String(_) => Type::Constcharp,
                Literal::Number(_) => Type::Unknown,
                Literal::Null => Type::Null,
            };
            Ok(Typed {
                typ,
                val: c::Expr::Literal(tr_literal(x)),
            })
        }
        nodes::Expression::NsName(n) => Ok(Typed {
            typ: Type::Unknown,
            val: c::Expr::Ident(tr_nsid(n, ctx)?),
        }),
        nodes::Expression::Identifier(x) => tr_ident(x, ctx),
        nodes::Expression::ArrayIndex(x) => tr_array_index(&x, ctx),
        nodes::Expression::BinaryOp(x) => tr_binary_op(&x, ctx),
        nodes::Expression::FieldAccess(x) => tr_field_access(&x, ctx),
        nodes::Expression::CompositeLiteral(x) => tr_comp_literal(&x, ctx),
        nodes::Expression::PrefixOperator(x) => tr_prefop(&x, ctx),
        nodes::Expression::PostfixOperator(x) => tr_postop(&x, ctx),
        nodes::Expression::Cast(x) => tr_cast(&x, ctx),
        nodes::Expression::FunctionCall(x) => tr_call(&x, ctx),
        nodes::Expression::Sizeof(x) => tr_sizeof(&x, ctx),
    };
}

fn tr_expr0(e: &nodes::Expression, ctx: &mut TrCtx) -> Result<c::Expr, BuildError> {
    tr_expr(e, ctx).map(|x| x.val)
}

fn tr_ident(x: &nodes::Identifier, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    mark_binding_use(ctx, &x.name);
    Ok(Typed {
        typ: Type::Unknown,
        val: c::Expr::Ident(x.name.clone()),
    })
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
                    nodes::Expression::FunctionCall(call) => {
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
            nodes::Statement::VariableDeclaration(x) => tr_vardecl(x, ctx)?,
            nodes::Statement::While(x) => tr_while(x, ctx)?,
        });
    }
    end_scope(ctx)?;
    Ok(c::CBody { statements })
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

fn tr_typedef(x: &nodes::Typedef, ctx: &mut TrCtx) -> Result<Vec<c::ModElem>, BuildError> {
    let array_size = &x.array_size;
    let alias = &x.alias;
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

    Ok(vec![c::ModElem::DefType(c::Typedef {
        is_pub: x.is_pub,
        type_name: tr_typename(type_name, ctx)?,
        form: c::CTypedefForm {
            stars: "*".repeat(*dereference_count),
            params,
            size: *array_size,
            alias: alias.name.clone(),
        },
    })])
}

// typedef { int a } foo_t;
fn tr_struct_typedef(
    x: &nodes::StructTypedef,
    ctx: &mut TrCtx,
) -> Result<Vec<c::ModElem>, BuildError> {
    // Translate to
    // "struct __foo_t_struct {int a}; typedef __foo_t_struct foo_t;".
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
                        type_name: tr_typename(&x.type_name, ctx)?,
                        form: tr_form(f, ctx)?,
                    }));
                }
            }
            nodes::StructEntry::Union(x) => {
                compat_fields.push(c::CStructItem::Union(tr_union(x, ctx)?));
            }
        }
    }
    Ok(vec![
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
    ])
}

// enum { A=1, B, C }
fn tr_enum(x: &nodes::Enum, ctx: &mut TrCtx) -> Result<Vec<c::ModElem>, BuildError> {
    let mut entries = Vec::new();
    for e in &x.entries {
        entries.push(c::EnumEntry {
            id: e.id.name.clone(),
            value: match &e.value {
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
fn tr_mod_var(x: &nodes::DeclVar, ctx: &mut TrCtx) -> Result<Vec<c::ModElem>, BuildError> {
    Ok(vec![c::ModElem::DeclVar(c::DeclVar {
        type_name: tr_typename(&x.type_name, ctx)?,
        form: tr_form(&x.form, ctx)?,
        value: tr_expr0(x.value.as_ref().unwrap(), ctx)?,
    })])
}

// *foo[]
fn tr_form(form: &nodes::Form, ctx: &mut TrCtx) -> Result<c::Form, BuildError> {
    let mut indexes = Vec::new();
    for index in &form.indexes {
        indexes.push(match index {
            Some(e) => Some(tr_expr0(e, ctx)?),
            None => None,
        })
    }
    Ok(c::Form {
        indexes,
        name: form.name.clone(),
        stars: "*".repeat(form.hops),
    })
}

fn tr_union(x: &nodes::Union, ctx: &mut TrCtx) -> Result<c::CUnion, BuildError> {
    let form = tr_form(&x.form, ctx)?;
    let mut fields = Vec::new();
    for f in &x.fields {
        fields.push(c::CUnionField {
            type_name: tr_typename(&f.type_name, ctx)?,
            form: tr_form(&f.form, ctx)?,
        });
    }
    Ok(c::CUnion { form, fields })
}

// <..> <op> <..>
fn tr_binary_op(x: &nodes::BinaryOp, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    let a = tr_expr(&x.a, ctx)?;
    let b = tr_expr(&x.b, ctx)?;

    Ok(Typed {
        typ: Type::Unknown,
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
            Type::Unknown
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
        typ: Type::Unknown,
        val: c::Expr::Cast {
            type_name: translate_anonymous_typeform(&x.type_name, ctx)?,
            operand: Box::new(operand.val),
        },
    })
}

// <..> [...]
fn tr_array_index(x: &nodes::ArrayIndex, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    let arr = tr_expr(&x.array, ctx)?;
    let ind = tr_expr(&x.index, ctx)?;
    Ok(Typed {
        typ: Type::Unknown,
        val: c::Expr::ArrayIndex {
            array: Box::new(arr.val),
            index: Box::new(ind.val),
        },
    })
}

// <..> -> field
// <..> . field
fn tr_field_access(x: &nodes::FieldAccess, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    let field_name = &x.field_name;
    let target = tr_expr(&x.target, ctx)?;
    Ok(Typed {
        typ: Type::Unknown,
        val: c::Expr::FieldAccess {
            op: x.op.clone(),
            target: Box::new(target.val),
            field_name: field_name.name.clone(),
        },
    })
}

// { .field = <...>, .field = <...>, ... }
fn tr_comp_literal(
    x: &nodes::CompositeLiteral,
    ctx: &mut TrCtx,
) -> Result<Typed<c::Expr>, BuildError> {
    let mut entries = Vec::new();
    for e in &x.entries {
        let key = match &e.key {
            Some(x) => {
                let tr = tr_expr(x, ctx)?;
                Some(tr.val)
            }
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
        typ: Type::Unknown,
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
        typ: Type::Unknown,
        val: c::Expr::Sizeof { arg: Box::new(arg) },
    })
}

// f(args)
fn tr_call(x: &nodes::FunctionCall, ctx: &mut TrCtx) -> Result<Typed<c::Expr>, BuildError> {
    let mut targs: Vec<c::Expr> = Vec::new();
    for arg in &x.args {
        let e = tr_expr(arg, ctx)?;
        targs.push(e.val)
    }
    let ft = tr_expr(&x.func, ctx)?;
    Ok(Typed {
        typ: Type::Unknown,
        val: c::Expr::Call {
            func: Box::new(ft.val),
            args: targs,
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
                type_name: tr_typename(&param.type_name, ctx)?,
                form: tr_form(form, ctx)?,
            })
        }
    }
    Ok(c::FuncParams {
        list: parameters,
        variadic: x.variadic,
    })
}

fn tr_func_decl(x: &nodes::DeclFunc, ctx: &mut TrCtx) -> Result<Vec<c::ModElem>, BuildError> {
    begin_scope(ctx);

    for p in &x.parameters.list {
        for f in &p.forms {
            add_binding(ctx, &f.name, f.pos.clone(), false);
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
    let mut r = vec![c::ModElem::DefFunc(c::FunctionDef {
        is_static: !x.is_pub,
        type_name: tr_typename(&x.type_name, ctx)?,
        form: tr_form(&x.form, ctx)?,
        parameters: tr_func_params(&x.parameters, ctx)?,
        body: rbody,
    })];
    if format_che::fmt_form(&x.form) != "main" {
        r.push(c::ModElem::ForwardFunc(c::ForwardFunc {
            is_static: !x.is_pub,
            type_name: tr_typename(&x.type_name, ctx)?,
            form: tr_form(&x.form, ctx)?,
            parameters: tr_func_params(&x.parameters, ctx)?,
        }));
    }
    end_scope(ctx)?;

    if (!is_void(&x.type_name) || x.form.hops == 1) && !body_returns(&x.body) {
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

// foo.bar() -> _foo_123__bar()
fn tr_nsid(nsid: &nodes::NsName, ctx: &mut TrCtx) -> Result<String, BuildError> {
    // "OS" is a special namespace that is simply omitted.
    if nsid.namespace == "OS" {
        return Ok(nsid.name.clone());
    }
    if nsid.namespace != "" {
        ctx.used_ns.insert(nsid.namespace.clone());
        // ns -> path
        // Get the module imported as this namespace.
        let pos = ctx
            .this_mod_head
            .imports
            .iter()
            .position(|x| x.ns == nsid.namespace)
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

        if !exports_has(&exports, &nsid.name) {
            return Err(BuildError {
                message: format!("{} doesn't have exported {}", &nsid.namespace, &nsid.name),
                pos: nsid.pos.fmt(),
                path: ctx.this_mod_head.filepath.clone(),
            });
        }

        return Ok(format!("{}__{}", id, nsid.name));
    }
    if !mark_binding_use(ctx, &nsid.name) {
        return Err(BuildError {
            message: format!("{} is undefined", &nsid.name),
            pos: nsid.pos.fmt(),
            path: ctx.this_mod_head.filepath.clone(),
        });
    }
    return Ok(nsid.name.clone());
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
                    add_binding(ctx, &form.name, form.pos.clone(), false);
                    c::ForInit::DeclLoopCounter(c::DeclVar {
                        type_name: tr_typename(type_name, ctx)?,
                        form: tr_form(form, ctx)?,
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
                nodes::SwitchCaseValue::Ident(x) => c::CSwitchCaseValue::Ident(tr_nsid(x, ctx)?),
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
        nodes::SwitchCaseValue::Ident(ns_name) => c::Expr::Ident(tr_nsid(&ns_name, ctx)?),
        nodes::SwitchCaseValue::Literal(literal) => c::Expr::Literal(tr_literal(&literal)),
    });
    args.push(switchval.clone());
    let mut root = mk_neg(mk_call0("strcmp", args));
    let n = sw.values.len();
    for i in 1..n {
        let mut args: Vec<c::Expr> = Vec::new();
        args.push(match &sw.values[i] {
            nodes::SwitchCaseValue::Ident(ns_name) => c::Expr::Ident(tr_nsid(&ns_name, ctx)?),
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
fn tr_vardecl(x: &nodes::DeclVar, ctx: &mut TrCtx) -> Result<c::Statement, BuildError> {
    add_binding(ctx, &x.form.name, x.form.pos.clone(), false);

    Ok(c::Statement::VariableDeclaration {
        type_name: tr_typename(&x.type_name, ctx)?,
        forms: vec![tr_form(&x.form, ctx)?],
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
    arguments: &Vec<nodes::Expression>,
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
