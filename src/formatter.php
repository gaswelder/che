<?php

function format_node($node)
{
    if (is_array($node)) {
        switch ($node['kind']) {
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
            default:
                throw new Exception("Unknown node type: " . $node['kind']);
        }
    }
    $cn = get_class($node);
    switch ($cn) {
        case c_anonymous_parameters::class:
            return format_anonymous_parameters($node);
        case c_anonymous_typeform::class:
            return format_anonymous_typeform($node);
        case c_array_index::class:
            return format_array_index($node);
        case c_array_literal_entry::class:
            return format_array_literal_entry($node);
        case c_array_literal::class:
            return format_array_literal($node);
        case c_binary_op::class:
            return format_binary_op($node);
        case c_body::class:
            return format_body($node);
        case c_cast::class:
            return format_cast($node);
        case c_compat_function_declaration::class:
            return format_compat_function_declaration($node);
        case c_compat_function_forward_declaration::class:
            return format_compat_function_forward_declaration($node);
        case c_compat_include::class:
            return format_compat_include($node);
        case c_compat_macro::class:
            return format_compat_macro($node);
        case c_compat_module::class:
            return format_compat_module($node);
        case c_compat_struct_definition::class:
            return format_compat_struct_definition($node);
        case c_compat_struct_forward_declaration::class:
            return format_compat_struct_forward_declaration($node);
        case c_composite_type::class:
            return format_composite_type($node);
        case c_defer::class:
            return format_defer($node);
        case c_ellipsis::class:
            return format_ellipsis($node);
        case c_enum_member::class:
            return format_enum_member($node);
        case c_compat_enum::class:
            return format_compat_enum($node);
        case c_enum::class:
            return format_enum($node);
        case c_form::class:
            return format_form($node);
        case c_for::class:
            return format_for($node);
        case c_function_arguments::class:
            return format_function_arguments($node);
        case c_function_call::class:
            return format_function_call($node);
        case c_function_declaration::class:
            return format_function_declaration($node);
        case c_function_parameter::class:
            return format_function_parameter($node);
        case c_function_parameters::class:
            return format_function_parameters($node);
        case c_identifier::class:
            return format_identifier($node);
        case c_if::class:
            return format_if($node);
        case c_import::class:
            return format_import($node);
        case c_literal::class:
            return format_literal($node);
        case c_module::class:
            return format_module($node);
        case c_module_variable::class:
            return format_module_variable($node);
        case c_postfix_operator::class:
            return format_postfix_operator($node);
        case c_prefix_operator::class:
            return format_prefix_operator($node);
        case c_return::class:
            return format_return($node);
        case c_sizeof::class:
            return format_sizeof($node);
        case c_struct_fieldlist::class:
            return format_struct_fieldlist($node);
        case c_struct_literal_member::class:
            return format_struct_literal_member($node);
        case c_struct_literal::class:
            return format_struct_literal($node);
        case c_switch::class:
            return format_switch($node);
        case c_typedef::class:
            return format_typedef($node);
        default:
            throw new Exception("don't know how to format '$cn'");
    }
}

function format_anonymous_parameters($node)
{
    $s = '(';
    foreach ($node->forms as $i => $form) {
        if ($i > 0) $s .= ', ';
        $s .= format_node($form);
    }
    $s .= ')';
    return $s;
}

function format_anonymous_typeform($node)
{
    $s = format_node($node->type);
    foreach ($node->ops as $op) {
        $s .= $op;
    }
    return $s;
}

function format_array_index($node)
{
    return format_node($node->array) . '[' . format_node($node->index) . ']';
}

function format_array_literal_entry($node)
{
    $s = '';
    if ($node->index) {
        $s .= '[' . format_node($node->index) . '] = ';
    }
    $s .= format_node($node->value);
    return $s;
}

function format_array_literal($node)
{
    $s = '{';
    foreach ($node->values as $i => $value) {
        if ($i > 0) $s .= ', ';
        $s .= format_node($value);
    }
    if (empty($node->values)) {
        $s .= '0';
    }
    $s .= '}';
    return $s;
}

function format_binary_op($node)
{
    $parts = [
        $node->brace_if_needed($node->a),
        $node->op,
        $node->brace_if_needed($node->b)
    ];
    if (in_array($node->op, ['.', '->'])) {
        return implode('', $parts);
    }
    return implode(' ', $parts);
}

function format_body($node)
{
    $s = '';
    foreach ($node->statements as $statement) {
        $s .= format_node($statement) . ";\n";
    }
    return "{\n" . indent($s) . "}\n";
}

function format_cast($node)
{
    return sprintf("(%s) %s", format_node($node->type), format_node($node->operand));
}

function format_compat_function_declaration($node)
{
    $s = '';
    if ($node->static && format_node($node->form) != 'main') {
        $s .= 'static ';
    }
    $s .= format_node($node->type)
        . ' ' . format_node($node->form)
        . format_node($node->parameters)
        . ' ' . format_node($node->body);
    return $s;
}

function format_compat_function_forward_declaration($node)
{
    $s = '';
    if ($node->static && format_node($node->form) != 'main') {
        $s .= 'static ';
    }
    $s .= format_node($node->type)
        . ' ' . format_node($node->form)
        . format_node($node->parameters) . ";\n";
    return $s;
}

function format_compat_include($node)
{
    return "#include $node->name\n";
}

function format_compat_macro($node)
{
    return $node->content . "\n";
}

function format_compat_module($node)
{
    $s = '';
    foreach ($node->elements as $node) {
        $s .= format_node($node);
    }
    return $s;
}

function format_compat_struct_definition($node)
{
    return 'struct ' . $node->name . ' ' . format_node($node->fields) . ";\n";
}

function format_compat_struct_forward_declaration($node)
{
    return 'struct ' . format_node($node->name) . ";\n";
}

function format_composite_type($node)
{
    $s = '';
    foreach ($node->fieldlists as $fieldlist) {
        $s .= format_node($fieldlist) . "\n";
    }
    return "{\n"
        . indent($s) . "}";
}

function format_defer($node)
{
    return 'defer ' . format_node($node->expression) . ';';
}

function format_ellipsis($node)
{
    return '...';
}

function format_enum_member($node)
{
    $s = format_node($node->id);
    if ($node->value) {
        $s .= ' = ' . format_node($node->value);
    }
    return $s;
}

function format_compat_enum($node)
{
    $s = "enum {\n";
    foreach ($node->members as $i => $member) {
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
    if ($node->pub) {
        $s .= 'pub ';
    }
    $s .= "enum {\n";
    foreach ($node->members as $id) {
        $s .= "\t" . format_node($id) . ",\n";
    }
    $s .= "}\n";
    return $s;
}

function format_form($node)
{
    return $node->str;
}

function format_for($node)
{
    return sprintf(
        'for (%s; %s; %s) %s',
        format_node($node->init),
        format_node($node->condition),
        format_node($node->action),
        format_node($node->body)
    );
}

function format_loop_counter_declaration($node)
{
    $s = sprintf(
        '%s %s = %s',
        format_node($node['type']),
        format_node($node['name']),
        format_node($node['value'])
    );
    return $s;
}

function format_function_arguments($node)
{
    $s = '(';
    foreach ($node->arguments as $i => $argument) {
        if ($i > 0) $s .= ', ';
        $s .= format_node($argument);
    }
    $s .= ')';
    return $s;
}

function format_function_call($node)
{
    return format_node($node->function) . format_node($node->arguments);
}

function format_function_declaration($node)
{
    $s = sprintf(
        "%s %s%s %s\n\n",
        format_node($node->type),
        format_node($node->form),
        format_node($node->parameters),
        format_node($node->body)
    );
    if ($node->pub) {
        return 'pub ' . $s;
    }
    return $s;
}

function format_function_parameter($node)
{
    $s = format_node($node->type) . ' ';
    foreach ($node->forms as $i => $form) {
        if ($i > 0) $s .= ', ';
        $s .= format_node($form);
    }
    return $s;
}

function format_function_parameters($node)
{
    $s = '(';
    foreach ($node->parameters as $i => $parameter) {
        if ($i > 0) $s .= ', ';
        $s .= format_node($parameter);
    }
    $s .= ')';
    return $s;
}

function format_identifier($node)
{
    return $node->name;
}

function format_if($node)
{
    $s = sprintf('if (%s) %s', format_node($node->condition), format_node($node->body));
    if ($node->else) {
        $s .= ' else ' . format_node($node->else);
    }
    return $s;
}

function format_import($node)
{
    return "import $node->path\n";
}

function format_literal($node)
{
    switch ($node->type) {
        case 'string':
            return '"' . $node->value . '"';
        case 'char':
            return '\'' . $node->value . '\'';
        default:
            return $node->value;
    }
}

function format_module($node)
{
    $s = '';
    foreach ($node->elements as $cnode) {
        $s .= format_node($cnode);
    }
    return $s;
}

function format_module_variable($node)
{
    return format_node($node->type)
        . ' ' . format_node($node->form)
        . ' = ' . format_node($node->value)
        . ";\n";
}

function format_postfix_operator($node)
{
    return format_node($node->operand) . $node->operator;
}

function format_prefix_operator($node)
{
    if ($node->operand instanceof c_binary_op || $node->operand instanceof c_cast) {
        return $node->operator . '(' . format_node($node->operand) . ')';
    }
    return $node->operator . format_node($node->operand);
}

function format_return($node)
{
    if (!$node->expression) {
        return 'return;';
    }
    return 'return ' . format_node($node->expression) . ';';
}

function format_sizeof($node)
{
    return 'sizeof(' . format_node($node->argument) . ')';
}

function format_struct_fieldlist($node)
{
    $s = format_node($node->type) . ' ';
    foreach ($node->forms as $i => $form) {
        if ($i > 0) $s .= ', ';
        $s .= format_node($form);
    }
    $s .= ';';
    return $s;
}

function format_struct_literal_member($node)
{
    return '.' . format_node($node->name) . ' = ' . format_node($node->value);
}

function format_struct_literal($node)
{
    $s = "{\n";
    foreach ($node->members as $member) {
        $s .= "\t" . format_node($member) . ",\n";
    }
    $s .= "}\n";
    return $s;
}

function format_switch($node)
{
    $s = '';
    foreach ($node->cases as $case) {
        $s .= 'case ' . format_node($case['value']) . ": {\n";
        foreach ($case['statements'] as $statement) {
            $s .= format_node($statement) . ";\n";
        }
        $s .= "}\n";
    }
    if ($node->default) {
        $s .= "default: {\n";
        foreach ($node->default as $statement) {
            $s .= format_node($statement) . ";\n";
        }
        $s .= "}\n";
    }
    return sprintf(
        "switch (%s) {\n%s\n}",
        format_node($node->value),
        indent($s)
    );
}

function format_typedef($node)
{
    return 'typedef ' . format_node($node->type) . ' '
        . $node->before
        . format_node($node->alias)
        . $node->after
        . ";\n";
}

function format_type($node)
{
    $s = '';
    if ($node['const']) {
        $s .= 'const ';
    }
    $s .= $node['type'];
    return $s;
}

function format_variable_declaration($node)
{
    $s = format_node($node['type']) . ' ';
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
        $s .= format_node($field['type']) . ' ' . format_node($field['form']) . ";\n";
    }
    return sprintf("union {\n%s\n} %s;\n", indent($s), format_node($node['form']));
}
