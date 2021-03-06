<?php

function format_node($node)
{
    if (is_array($node)) {
        switch ($node['kind']) {
            case 'c_body':
                return format_body($node);
            case 'c_while':
                return format_while($node);
            case 'c_variable_declaration':
                return format_variable_declaration($node);
            case 'c_union':
                return format_union($node);
            case 'c_loop_counter_declaration':
                return format_loop_counter_declaration($node);
            case 'c_type':
                return format_type($node);
            case 'c_sizeof':
                return format_sizeof($node);
            case 'c_return':
                return format_return($node);
            case 'c_typedef':
                return format_typedef($node);
            case 'c_switch':
                return format_switch($node);
            case 'c_module_variable':
                return format_module_variable($node);
            case 'c_struct_literal':
                return format_struct_literal($node);
            case 'c_for':
                return format_for($node);
            case 'c_if':
                return format_if($node);
            case 'c_struct_fieldlist':
                return format_struct_fieldlist($node);
            case 'c_anonymous_typeform':
                return format_anonymous_typeform($node);
            case 'c_identifier':
                return $node['name'];
            case 'c_compat_macro':
                return "#$node[name] $node[value]\n";
            case 'c_prefix_operator':
                return format_prefix_operator($node);
            case 'c_postfix_operator':
                return format_postfix_operator($node);
            case 'c_module':
                return format_module($node);
            case 'c_compat_module':
                return format_compat_module($node);
            case 'c_literal':
                return format_literal($node);
            case 'c_import':
                return format_import($node);
            case 'c_function_declaration':
                return format_function_declaration($node);
            case 'c_function_call':
                return format_function_call($node);
            case 'c_array_index':
                return format_array_index($node);
            case 'c_array_literal':
                return format_array_literal($node);
            case 'c_cast':
                return format_cast($node);
            case 'c_compat_include':
                return format_compat_include($node);
            case 'c_enum_member':
                return format_enum_member($node);
            case 'c_compat_enum':
                return format_compat_enum($node);
            case 'c_anonymous_struct':
                return format_anonymous_struct($node);
            case 'c_form':
                return format_form($node);
            case 'c_compat_struct_forward_declaration':
                return format_compat_struct_forward_declaration($node);
            case 'c_compat_function_forward_declaration':
                return format_compat_function_forward_declaration($node);
            case 'c_compat_struct_definition':
                return format_compat_struct_definition($node);
            case 'c_binary_op':
                return format_binary_op($node);
            case 'c_anonymous_parameters':
                return format_anonymous_parameters($node);
            case 'c_enum':
                return format_enum($node);
            case 'c_compat_function_declaration':
                return format_compat_function_declaration($node);
            default:
                throw new Exception("Unknown node type: " . $node['kind']);
        }
    }
}

function format_anonymous_parameters($node)
{
    $s = '(';
    foreach ($node['forms'] as $i => $form) {
        if ($i > 0) $s .= ', ';
        $s .= format_node($form);
    }
    $s .= ')';
    return $s;
}

function format_anonymous_typeform($node)
{
    $s = format_node($node['type_name']);
    foreach ($node['ops'] as $op) {
        $s .= $op;
    }
    return $s;
}

function format_array_index($node)
{
    return format_node($node['array']) . '[' . format_node($node['index']) . ']';
}

function format_array_literal($node)
{
    $s = '{';
    foreach ($node['values'] as $i => $entry) {
        if ($i > 0) {
            $s .= ', ';
        }
        $s1 = '';
        if ($entry['index']) {
            $s1 .= '[' . format_node($entry['index']) . '] = ';
        }
        $s1 .= format_node($entry['value']);
        $s .= $s1;
    }
    if (empty($node['values'])) {
        $s .= '0';
    }
    $s .= '}';
    return $s;
}

function brace_if_needed($node, $operand)
{
    if (
        is_array($operand) &&
        $operand['kind'] === 'c_binary_op' &&
        operator_strength($operand['op']) < operator_strength($node['op'])
    ) {
        return '(' . format_node($operand) . ')';
    }
    return format_node($operand);
}

function format_binary_op($node)
{
    $parts = [
        brace_if_needed($node, $node['a']),
        $node['op'],
        brace_if_needed($node, $node['b'])
    ];
    if (in_array($node['op'], ['.', '->'])) {
        return implode('', $parts);
    }
    return implode(' ', $parts);
}

function format_body($node)
{
    $s = '';
    foreach ($node['statements'] as $statement) {
        $s .= format_node($statement) . ";\n";
    }
    return "{\n" . indent($s) . "}\n";
}

function format_cast($node)
{
    return sprintf("(%s) %s", format_node($node['type_name']), format_node($node['operand']));
}

function format_compat_function_declaration($node)
{
    $s = '';
    if ($node['static'] && format_node($node['form']) != 'main') {
        $s .= 'static ';
    }
    $s .= format_node($node['type_name'])
        . ' ' . format_node($node['form'])
        . format_function_parameters($node['parameters'])
        . ' ' . format_node($node['body']);
    return $s;
}

function format_compat_function_forward_declaration($node)
{
    $s = '';
    if ($node['static'] && format_node($node['form']) != 'main') {
        $s .= 'static ';
    }
    $s .= format_node($node['type_name'])
        . ' ' . format_node($node['form'])
        . format_function_parameters($node['parameters']) . ";\n";
    return $s;
}

function format_compat_include($node)
{
    return "#include $node[name]\n";
}

function format_compat_module($node)
{
    $s = '';
    foreach ($node['elements'] as $node) {
        $s .= format_node($node);
    }
    return $s;
}

function format_compat_struct_definition($node)
{
    return 'struct ' . $node['name'] . ' ' . format_node($node['fields']) . ";\n";
}

function format_compat_struct_forward_declaration($node)
{
    return 'struct ' . $node['name'] . ";\n";
}

function format_anonymous_struct($node)
{
    $s = '';
    foreach ($node['fieldlists'] as $fieldlist) {
        $s .= format_node($fieldlist) . "\n";
    }
    return "{\n"
        . indent($s) . "}";
}

function format_enum_member($node)
{
    $s = format_node($node['id']);
    if ($node['value']) {
        $s .= ' = ' . format_node($node['value']);
    }
    return $s;
}

function format_compat_enum($node)
{
    $s = "enum {\n";
    foreach ($node['members'] as $i => $member) {
        if ($i > 0) {
            $s .= ",\n";
        }
        $s .= "\t" . format_node($member);
    }
    $s .= "\n};\n";
    return $s;
}

function format_enum($node)
{
    $s = '';
    if ($node['is_pub']) {
        $s .= 'pub ';
    }
    $s .= "enum {\n";
    foreach ($node['members'] as $id) {
        $s .= "\t" . format_node($id) . ",\n";
    }
    $s .= "}\n";
    return $s;
}

function format_form($node)
{
    $s = $node['stars'] . $node['name'];
    foreach ($node['indexes'] as $expr) {
        if ($expr) {
            $s .= '[' . format_node($expr) . ']';
        } else {
            $s .= '[]';
        }
    }
    return $s;
}

function format_for($node)
{
    return sprintf(
        'for (%s; %s; %s) %s',
        format_node($node['init']),
        format_node($node['condition']),
        format_node($node['action']),
        format_node($node['body'])
    );
}

function format_loop_counter_declaration($node)
{
    $s = sprintf(
        '%s %s = %s',
        format_node($node['type_name']),
        format_node($node['name']),
        format_node($node['value'])
    );
    return $s;
}

function format_function_call($node)
{
    $s1 = '(';
    foreach ($node['arguments'] as $i => $argument) {
        if ($i > 0) {
            $s1 .= ', ';
        }
        $s1 .= format_node($argument);
    }
    $s1 .= ')';
    return format_node($node['function']) . $s1;
}

function format_function_declaration($node)
{
    $s = sprintf(
        "%s %s%s %s\n\n",
        format_node($node['type_name']),
        format_node($node['form']),
        format_function_parameters($node['parameters']),
        format_node($node['body'])
    );
    if ($node->pub) {
        return 'pub ' . $s;
    }
    return $s;
}

function format_function_parameters($parameters)
{
    $s = '(';
    foreach ($parameters['list'] as $i => $parameter) {
        if ($i > 0) {
            $s .= ', ';
        }
        $s1 = format_node($parameter['type_name']) . ' ';
        foreach ($parameter['forms'] as $i => $form) {
            if ($i > 0) {
                $s1 .= ', ';
            }
            $s1 .= format_node($form);
        }
        $s .= $s1;
    }
    if ($parameters['variadic']) {
        $s .= ', ...';
    }
    $s .= ')';
    return $s;
}

function format_if($node)
{
    $s = sprintf('if (%s) %s', format_node($node['condition']), format_node($node['body']));
    if ($node['else_body']) {
        $s .= ' else ' . format_node($node['else_body']);
    }
    return $s;
}

function format_import($node)
{
    return "import $node[path]\n";
}

function format_literal($node)
{
    switch ($node['type_name']) {
        case 'string':
            return '"' . $node['value'] . '"';
        case 'char':
            return '\'' . $node['value'] . '\'';
        default:
            return $node['value'];
    }
}

function format_module($node)
{
    $s = '';
    foreach ($node['elements'] as $cnode) {
        $s .= format_node($cnode);
    }
    return $s;
}

function format_module_variable($node)
{
    return format_node($node['type_name'])
        . ' ' . format_node($node['form'])
        . ' = ' . format_node($node['value'])
        . ";\n";
}

function format_postfix_operator($node)
{
    return format_node($node['operand']) . $node['operator'];
}

function format_prefix_operator($node)
{
    $operand = $node['operand'];
    $operator = $node['operator'];
    if (is_array($operand) && ($operand['kind'] === 'c_binary_op' || $operand['kind'] === 'c_cast')) {
        return $operator . '(' . format_node($operand) . ')';
    }
    return $operator . format_node($operand);
}

function format_return($node)
{
    if (!$node['expression']) {
        return 'return;';
    }
    return 'return ' . format_node($node['expression']) . ';';
}

function format_sizeof($node)
{
    return 'sizeof(' . format_node($node['argument']) . ')';
}

function format_struct_fieldlist($node)
{
    $s = format_node($node['type_name']) . ' ';
    foreach ($node['forms'] as $i => $form) {
        if ($i > 0) $s .= ', ';
        $s .= format_node($form);
    }
    $s .= ';';
    return $s;
}

function format_struct_literal($node)
{
    $s = "{\n";
    foreach ($node['members'] as $member) {
        $s .= "\t" . '.' . format_node($member['name']) . ' = ' . format_node($member['value']) . ",\n";
    }
    $s .= "}\n";
    return $s;
}

function format_switch($node)
{
    $s = '';
    foreach ($node['cases'] as $case) {
        $s .= 'case ' . format_node($case['value']) . ": {\n";
        foreach ($case['statements'] as $statement) {
            $s .= format_node($statement) . ";\n";
        }
        $s .= "}\n";
    }
    if ($node['default']) {
        $s .= "default: {\n";
        foreach ($node['default'] as $statement) {
            $s .= format_node($statement) . ";\n";
        }
        $s .= "}\n";
    }
    return sprintf(
        "switch (%s) {\n%s\n}",
        format_node($node['value']),
        indent($s)
    );
}

function format_typedef($node)
{
    $form = $node['form']['stars']
        . format_node($node['form']['alias']);
    if ($node['form']['params']) {
        $form .= format_anonymous_parameters($node['form']['params']);
    }
    if ($node['form']['size']) {
        $form .= '[' . $node['form']['size'] . ']';
    }
    $type = format_node($node['type_name']);
    return "typedef $type $form;\n";
}

function format_type($node)
{
    $s = '';
    if ($node['is_const']) {
        $s .= 'const ';
    }
    $s .= $node['type_name'];
    return $s;
}

function format_variable_declaration($node)
{
    $s = format_node($node['type_name']) . ' ';
    foreach ($node['forms'] as $i => $form) {
        $value = $node['values'][$i];
        if ($i > 0) $s .= ', ';
        $s .= format_node($form) . ' = ' . format_node($value);
    }
    $s .= ";\n";
    return $s;
}

function format_while($node)
{
    return sprintf('while (%s) %s', format_node($node['condition']), format_node($node['body']));
}

function format_union($node)
{
    $s = '';
    foreach ($node['fields'] as $field) {
        $s .= format_node($field['type_name']) . ' ' . format_node($field['form']) . ";\n";
    }
    return sprintf("union {\n%s\n} %s;\n", indent($s), format_node($node['form']));
}
