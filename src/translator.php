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
    if ($cn === 'c_function_declaration') {
        return translate_function_declaration($node);
    }
    if ($cn === 'c_enum') {
        return translate_enum($node);
    }
    if ($cn === 'c_function_parameters') {
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
        'c_compat_struct_definition',
        'c_compat_struct_forward_declaration',
        'c_compat_macro',
    ];

    foreach ($module['elements'] as $element) {
        if ($element instanceof c_compat_function_declaration && !$element->static) {
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
        if ($cn === 'c_compat_enum' && !$element['hidden']) {
            $elements[] = $element;
            continue;
        }
    }
    return $elements;
}

function translate_enum($node)
{
    return [[
        'kind' => 'c_compat_enum',
        'members' => $node['members'],
        'hidden' => !$node['pub']
    ]];
}

function translate_typedef($node)
{
    if (is_array($node['type']) && $node['type']['kind'] === 'c_composite_type') {
        $struct_name = '__' . format_node($node['alias']) . '_struct';
        return [
            [
                'kind' => 'c_compat_struct_forward_declaration',
                'name' => $struct_name
            ],
            [
                'kind' => 'c_compat_struct_definition',
                'name' => $struct_name,
                'fields' => $node['type']
            ],
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

function translate_function_declaration($node)
{
    $func = new c_compat_function_declaration(
        !$node['pub'],
        $node['type'],
        $node['form'],
        translate_function_parameters($node['parameters']),
        $node['body']
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
        $elements[] = [
            'kind' => 'c_compat_include',
            'name' => "<$n.h>"
        ];
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
        if (in_array($cn, ['c_compat_include', 'c_compat_macro'])) {
            return 0;
        }
        if ($cn === 'c_compat_struct_forward_declaration') {
            return 1;
        }
        if ($cn === 'c_typedef') {
            return 2;
        }
        if (in_array($cn, ['c_compat_struct_definition', 'c_compat_enum'])) {
            return 3;
        }
        if ($cn === 'c_compat_function_forward_declaration') {
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
        if (
            is_array($element)
            && ($element['kind'] === 'c_typedef'
                || $element['kind'] === 'c_compat_struct_definition')
        ) {
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
    $parameters = [];
    foreach ($node['parameters'] as $parameter) {
        if (is_array($parameter) && isset($parameter['kind']) && $parameter['kind'] === 'c_ellipsis') {
            $parameters[] = $parameter;
            continue;
        }
        $parameters = array_merge($parameters, translate_function_parameter($parameter));
    }
    return [
        'kind' => 'c_function_parameters',
        'parameters' => $parameters
    ];
}

function translate_function_parameter($node)
{
    $compat = [];
    foreach ($node['forms'] as $form) {
        $type = $node['type'];
        $forms = [$form];
        $compat[] = [
            'type' => $type,
            'forms' => $forms
        ];
    }
    return $compat;
}
