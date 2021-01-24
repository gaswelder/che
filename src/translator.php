<?php

function translate(c_module $m)
{
    [$elements, $link] = translate_module($m->elements());
    return new c_compat_module($elements, $link);
}

function translate_node($node)
{
    if ($node instanceof c_import) {
        $module = resolve_import($node);
        $compat = translate($module);
        return $compat->synopsis();
    }
    if ($node instanceof c_function_declaration) {
        $func = $node->translate();
        return [
            $func->forward_declaration(),
            $func
        ];
    }
    if ($node instanceof c_typedef) {
        return $node->translate();
    }
    if ($node instanceof c_enum) {
        return [$node->translate()];
    }
    // Discard #type hints.
    if ($node instanceof c_compat_macro && $node->name() == 'type') {
        return [];
    }
    // Discard #link hints.
    if ($node instanceof c_compat_macro && $node->name() == 'link') {
        return [];
    }
    return [$node];
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
    $order = [
        [c_compat_include::class, c_compat_macro::class],
        [c_compat_struct_forward_declaration::class],
        [c_typedef::class],
        [c_compat_struct_definition::class, c_compat_enum::class],
        [c_compat_function_forward_declaration::class]
    ];

    $get_order = function ($element) use ($order) {
        foreach ($order as $i => $classnames) {
            if (in_array(get_class($element), $classnames)) {
                return $i;
            }
        }
        return count($order);
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
