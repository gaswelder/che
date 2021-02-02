<?php

function translate($module)
{
    [$elements, $link] = translate_module($module['elements']);
    return [
        'kind' => 'c_compat_module',
        'elements' => $elements,
        'link' => $link
    ];
}

function translate_node($node)
{
    $cn = is_array($node) ? $node['kind'] : get_class($node);

    if (is_array($node) && $node['kind'] == 'c_typedef') {
        return translate_typedef($node);
    }
    if ($cn === 'c_import') {
        return translate_import($node);
    }
    if ($node instanceof c_function_declaration) {
        return translate_function_declaration($node);
    }
    if ($node instanceof c_enum) {
        return translate_enum($node);
    }
    if ($node instanceof c_function_parameters) {
        return translate_function_parameters($node);
    }
    if ($cn === 'c_compat_macro' && $node['name'] == 'type') {
        return [];
    }
    if ($cn === 'c_compat_macro' && $node['name'] == 'link') {
        return [];
    }
    return [$node];
}

function translate_import($node)
{
    $module = resolve_import($node);
    $compat = translate($module);
    return get_module_synopsis($compat);
}

function get_module_synopsis($module)
{
    $elements = [];

    $exports = [
        'c_typedef',
        c_compat_struct_definition::class,
        c_compat_struct_forward_declaration::class,
        'c_compat_macro',
    ];

    foreach ($module['elements'] as $element) {
        if ($element instanceof c_compat_function_declaration && !$element->is_static()) {
            $elements[] = $element->forward_declaration();
            continue;
        }
        if (is_array($element)) {
            $cn = $element['kind'];
        } else {
            $cn = get_class($element);
        }
        if (in_array($cn, $exports)) {
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
    return [new c_compat_enum($node->members, !$node->pub)];
}

function translate_typedef($node)
{
    if ($node['type'] instanceof c_composite_type) {
        $struct_name = '__' . format_node($node['alias']) . '_struct';
        return [
            new c_compat_struct_forward_declaration($struct_name),
            new c_compat_struct_definition($struct_name, $node['type']),
            [
                'kind' => 'c_typedef',
                'before' => null,
                'after' => null,
                'type' => [
                    'kind' => 'c_type',
                    'const' => false,
                    'type' => 'struct ' . $struct_name
                ],
                'alias' => $node['alias']
            ]
        ];
    }
    return [$node];
}

function translate_function_declaration(c_function_declaration $node)
{
    $func = new c_compat_function_declaration(
        !$node->pub,
        $node->type,
        $node->form,
        translate_function_parameters($node->parameters),
        $node->body
    );
    return [
        $func->forward_declaration(),
        $func
    ];
}

function translate_module($che_elements)
{
    $link = [];
    foreach ($che_elements as $node) {
        if (is_array($node) && $node['kind'] === 'c_compat_macro' && $node['name'] == 'link') {
            $link[] = $node['value'];
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
        if (is_array($element)) {
            $cn = $element['kind'];
        } else {
            $cn = get_class($element);
        }
        if (in_array($cn, [c_compat_include::class, 'c_compat_macro'])) {
            return 0;
        }
        if ($cn === c_compat_struct_forward_declaration::class) {
            return 1;
        }
        if ($cn === 'c_typedef') {
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
        if ((is_array($element) && $element['kind'] === 'c_typedef') || $element instanceof c_compat_struct_definition) {
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

function translate_function_parameters($node)
{
    $compat = new c_function_parameters;
    foreach ($node->parameters as $parameter) {
        if ($parameter instanceof c_ellipsis) {
            $compat->parameters[] = $parameter;
            continue;
        }
        $compat->parameters = array_merge($compat->parameters, translate_function_parameter($parameter));
    }
    return $compat;
}

function translate_function_parameter($node)
{
    $compat = [];
    foreach ($node->forms as $form) {
        $p = new c_function_parameter;
        $p->type = $node->type;
        $p->forms = [$form];
        $compat[] = $p;
    }
    return $compat;
}
