<?php

function translate(c_module $m)
{
    [$elements, $link] = translate_module($m->elements());
    return new c_compat_module($elements, $link);
}

function translate_module($che_elements)
{
    $elements = [];
    $link = [];

    foreach ($che_elements as $element) {
        if ($element instanceof c_import) {
            $module = resolve_import($element);
            $compat = translate($module);
            $elements = array_merge($elements, $compat->synopsis());
            continue;
        }
        if ($element instanceof c_function_declaration) {
            $func = $element->translate();
            $elements[] = $func->forward_declaration();
            $elements[] = $func;
            continue;
        }
        if ($element instanceof c_typedef) {
            $elements = array_merge($elements, $element->translate());
            continue;
        }
        if ($element instanceof c_enum) {
            $elements[] = $element->translate();
            continue;
        }
        // Discard #type hints.
        if ($element instanceof c_compat_macro && $element->name() == 'type') {
            continue;
        }
        // Discard #link hints, but remember the values.
        if ($element instanceof c_compat_macro && $element->name() == 'link') {
            $link[] = $element->value();
            continue;
        }
        $elements[] = $element;
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
            $s = $element->format();
            if (isset($set[$s])) {
                continue;
            }
            $set[$s] = true;
        }
        $result[] = $element;
    }

    return $result;
}