<?php

function translate(c_module $m)
{
    [$elements, $link] = translate_module($m->elements());
    return new c_compat_module($elements, $link);
}

function translate_node($node)
{
    if ($node instanceof c_import) {
        return translate_import($node);
    }
    if ($node instanceof c_function_declaration) {
        return translate_function_declaration($node);
    }
    if ($node instanceof c_typedef) {
        return translate_typedef($node);
    }
    if ($node instanceof c_enum) {
        return translate_enum($node);
    }
    if ($node instanceof c_compat_macro && $node->name() == 'type') {
        return [];
    }
    if ($node instanceof c_compat_macro && $node->name() == 'link') {
        return [];
    }
    return [$node];
}

function translate_import(c_import $node)
{
    $module = resolve_import($node);
    $compat = translate($module);
    return get_module_synopsis($compat);
}

function get_module_synopsis(c_compat_module $node)
{
    $elements = [];

    $exports = [
        c_typedef::class,
        c_compat_struct_definition::class,
        c_compat_struct_forward_declaration::class,
        c_compat_macro::class,
    ];

    foreach ($node->elements as $element) {
        if ($element instanceof c_compat_function_declaration && !$element->is_static()) {
            $elements[] = $element->forward_declaration();
            continue;
        }
        if (in_array(get_class($element), $exports)) {
            $elements[] = $element;
            continue;
        }
        if ($element instanceof c_compat_enum && !$element->is_private()) {
            $elements[] = $element;
            continue;
        }
    }
    return $elements;
}

function translate_enum(c_enum $node)
{
    return [$node->translate()];
}

function translate_typedef(c_typedef $node)
{
    if ($node->type instanceof c_composite_type) {
        $struct_name = '__' . $node->name() . '_struct';
        $typedef = new c_typedef;
        $typedef->alias = $node->alias;
        $typedef->type = [
            'kind' => 'c_type',
            'const' => false,
            'type' => 'struct ' . $struct_name
        ];
        return [
            new c_compat_struct_forward_declaration(c_identifier::make($struct_name)),
            new c_compat_struct_definition($struct_name, $node->type),
            $typedef
        ];
    }
    return [$node];
}

function translate_function_declaration(c_function_declaration $node)
{
    $func = $node->translate();
    return [
        $func->forward_declaration(),
        $func
    ];
}

function translate_module($che_elements)
{
    $link = [];
    foreach ($che_elements as $node) {
        if ($node instanceof c_compat_macro && $node->name() == 'link') {
            $link[] = $node->value();
        }
    }

    $elements = [];
    foreach ($che_elements as $element) {
        $elements = array_merge($elements, translate_node($element));
    }

    $std = [
        'assert',
        'ctype',
        'errno',
        'limits',
        'math',
        'stdarg',
        'stdbool',
        'stddef',
        'stdint',
        'stdio',
        'stdlib',
        'string',
        'time'
    ];
    foreach ($std as $n) {
        $elements[] = new c_compat_include("<$n.h>");
    }

    return [deduplicate_synopsis(hoist_declarations($elements)), $link];
}

function hoist_declarations($elements)
{
    $get_order = function ($element) {
        $cn = get_class($element);
        if (in_array($cn, [c_compat_include::class, c_compat_macro::class])) {
            return 0;
        }
        if ($cn === c_compat_struct_forward_declaration::class) {
            return 1;
        }
        if ($cn === c_typedef::class) {
            return 2;
        }
        if (in_array($cn, [c_compat_struct_definition::class, c_compat_enum::class])) {
            return 3;
        }
        if ($cn === c_compat_function_forward_declaration::class) {
            return 4;
        }
        return 5;
    };

    $groups = [[], [], [], [], []];

    foreach ($elements as $element) {
        $groups[$get_order($element)][] = $element;
    }
    return call_user_func_array('array_merge', $groups);
}

function deduplicate_synopsis($elements)
{
    $result = [];
    $set = [];

    foreach ($elements as $element) {
        if ($element instanceof c_typedef || $element instanceof c_compat_struct_definition) {
            $s = format_node($element);
            if (isset($set[$s])) {
                continue;
            }
            $set[$s] = true;
        }
        $result[] = $element;
    }

    return $result;
}
