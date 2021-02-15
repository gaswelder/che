<?php

function parse_module($lexer)
{
    $m = [
        'kind' => 'c_module',
        'elements' => []
    ];
    while ($lexer->more()) {
        $m['elements'][] = parse_module_element($lexer);
    }
    return $m;
}

function parse_module_element($lexer)
{
    switch ($lexer->peek()['kind']) {
        case 'import':
            $import = parse_import($lexer);
            $m = resolve_import($import);
            $lexer->typenames = array_merge($lexer->typenames, module_types($m));
            return $import;
        case 'typedef':
            return parse_typedef($lexer, $lexer->typenames);
        case 'macro':
            return parse_compat_macro($lexer);
    }

    $typenames = $lexer->typenames;

    $pub = false;
    if ($lexer->follows('pub')) {
        $lexer->get();
        $pub = true;
    }
    if ($lexer->follows('enum')) {
        return parse_enum($lexer, $pub);
    }
    try {
        $type = parse_type($lexer);
    } catch (Exception $e) {
        throw new Exception("unexpected input (expecting function, variable, typedef, struct, enum) || " . $e->getMessage());
    }
    $form = parse_form($lexer, $typenames);
    if ($lexer->peek()['kind'] == '(') {
        return parse_function_declaration($lexer, $pub, $type, $form, $typenames);
    }

    if ($lexer->peek()['kind'] != '=') {
        throw new Exception("module variable: '=' expected");
    }

    if ($lexer->peek()['kind'] == '=') {
        if ($pub) {
            throw new Exception("module variables can't be exported");
        }
        $lexer->get();
        $value = parse_expression($lexer, $typenames, 0);
        expect($lexer, ';', 'module variable declaration');
        return [
            'kind' => 'c_module_variable',
            'type_name' => $type,
            'form' => $form,
            'value' => $value
        ];
    }
    throw new Exception("unexpected input");
}

function parse_anonymous_parameters($lexer)
{
    return call_rust('parse_anonymous_parameters', $lexer);
}

function parse_import($lexer)
{
    expect($lexer, 'import');
    $tok = expect($lexer, 'string');
    $path = $tok['content'];
    return [
        'kind' => 'c_import',
        'path' => $path
    ];
}

function parse_typedef($lexer, $typenames)
{
    expect($lexer, 'typedef');

    $type = null;
    $alias = null;
    $before = null;
    $after = null;

    if ($lexer->follows('{')) {
        $type = parse_composite_type($lexer, $typenames);
        $alias = parse_identifier($lexer);
        expect($lexer, ';', 'typedef');
    } else {
        $type = parse_type($lexer, 'typedef');
        while ($lexer->follows('*')) {
            $before .= $lexer->get()['kind'];
        }
        $alias = parse_identifier($lexer);

        if ($lexer->follows('(')) {
            $after .= format_node(parse_anonymous_parameters($lexer));
        }

        if ($lexer->follows('[')) {
            $lexer->get();
            $after .= '[';
            $after .= expect($lexer, 'num')['content'];
            expect($lexer, ']');
            $after .= ']';
        }
        expect($lexer, ';', 'typedef');
    }

    return [
        'kind' => 'c_typedef',
        'type_name' => $type,
        'before' => $before,
        'after' => $after,
        'alias' => $alias
    ];
}

function parse_statement($lexer, $typenames)
{
    $next = $lexer->peek();
    if (
        ($next['kind'] == 'word' && is_type($next['content'], $typenames))
        || $next['kind'] == 'const'
    ) {
        return parse_variable_declaration($lexer, $typenames);
    }

    switch ($next['kind']) {
        case 'if':
            return parse_if($lexer, $typenames);
        case 'for':
            return parse_for($lexer, $typenames);
        case 'while':
            return parse_while($lexer, $typenames);
        case 'return':
            return parse_return($lexer, $typenames);
        case 'switch':
            return parse_switch($lexer, $typenames);
    }

    $expr = parse_expression($lexer, $typenames, 0);
    expect($lexer, ';', 'parsing statement');
    return $expr;
}

function parse_expression($lexer, $typenames, $current_strength)
{
    return call_rust('parse_expression', $lexer, $typenames, $current_strength);
}

function parse_binary_op($lexer, $typenames, $first_operand)
{
    $op = $lexer->get()['kind'];
    $second_operand = parse_expression($lexer, $typenames, operator_strength($op));
    return [
        'kind' => 'c_binary_op',
        'op' => $op,
        'a' => $first_operand,
        'b' => $second_operand
    ];
}

function parse_atom($lexer, $typenames)
{
    $nono = ['case', 'default', 'if', 'else', 'for', 'while', 'switch'];
    if (in_array($lexer->peek()['kind'], $nono)) {
        throw new Exception("expression: unexpected keyword '" . $lexer->peek()['kind'] . "'");
    }

    if (
        $lexer->peek()['kind'] == '('
        && $lexer->peek_n(1)['kind'] == 'word'
        && is_type($lexer->peek_n(1)['content'], $typenames)
    ) {
        expect($lexer, '(');
        $typeform = parse_anonymous_typeform($lexer);
        expect($lexer, ')', 'typecast');
        return [
            'kind' => 'c_cast',
            'type_name' => $typeform,
            'operand' => parse_expression($lexer, $typenames, 0)
        ];
    }

    if ($lexer->peek()['kind'] == '(') {
        $lexer->get();
        $expr = parse_expression($lexer, $typenames, 0);
        expect($lexer, ')');
        return $expr;
    }

    if ($lexer->peek()['kind'] == '{') {
        if ($lexer->peek_n(1)['kind'] == '.') {
            return parse_struct_literal($lexer, $typenames);
        }
        return parse_array_literal($lexer);
    }

    if ($lexer->peek()['kind'] == 'sizeof') {
        return parse_sizeof($lexer, $typenames);
    }

    if (is_prefix_op($lexer->peek()['kind'])) {
        $op = $lexer->get()['kind'];
        $operand = parse_expression($lexer, $typenames, operator_strength('prefix'));
        return [
            'kind' => 'c_prefix_operator',
            'operator' => $op,
            'operand' => $operand
        ];
    }

    if ($lexer->peek()['kind'] == 'word') {
        $result = parse_identifier($lexer);
    } else {
        $result = parse_literal($lexer);
    }

    while ($lexer->more()) {
        if ($lexer->peek()['kind'] == '(') {
            $result = parse_function_call($lexer, $typenames, $result);
            continue;
        }
        if ($lexer->peek()['kind'] == '[') {
            expect($lexer, '[', 'array index');
            $index = parse_expression($lexer, $typenames, 0);
            expect($lexer, ']', 'array index');
            $result = [
                'kind' => 'c_array_index',
                'array' => $result,
                'index' => $index
            ];
            continue;
        }

        if (is_postfix_op($lexer->peek()['kind'])) {
            $op = $lexer->get()['kind'];
            $result = [
                'kind' => 'c_postfix_operator',
                'operand' => $result,
                'operator' => $op
            ];
            continue;
        }
        break;
    }

    return $result;
}

function expect($lexer, $type, $comment = null)
{
    return call_rust('expect', $lexer, $type, $comment);
}

function is_type($name, $typenames)
{
    return call_rust_mem('is_type', $name, $typenames);
}

function is_op($token_type)
{
    return call_rust_mem("is_op", $token_type);
}

function is_prefix_op($op)
{
    return call_rust_mem("is_prefix_op", $op);
}

function is_postfix_op($op)
{
    return call_rust_mem("is_postfix_op", $op);
}

function operator_strength($op)
{
    return call_rust_mem("operator_strength", $op);
}

function parse_array_literal($lexer)
{
    return call_rust('parse_array_literal', $lexer);
}

function parse_array_literal_entry($lexer)
{
    return call_rust('parse_array_literal_entry', $lexer);
}

function parse_body($lexer, $typenames)
{
    $statements = [];
    if ($lexer->follows('{')) {
        expect($lexer, '{');
        while (!$lexer->follows('}')) {
            $statements[] = parse_statement($lexer, $typenames);
        }
        expect($lexer, '}');
    } else {
        $statements[] = parse_statement($lexer, $typenames);
    }
    return [
        'kind' => 'c_body',
        'statements' => $statements
    ];
}

function parse_compat_macro($lexer)
{
    $content = expect($lexer, 'macro')['content'];
    $pos = strpos($content, ' ');
    if ($pos === false) {
        throw new Exception("can't get macro name from '$content'");
    }
    $name = substr($content, 1, $pos - 1);
    $value = substr($content, $pos + 1);

    return [
        'kind' => 'c_compat_macro',
        'name' => $name,
        'value' => $value
    ];
}

function parse_composite_type($lexer, $typenames)
{
    $fieldlists = [];
    expect($lexer, '{', 'struct type definition');
    while ($lexer->more() && $lexer->peek()['kind'] != '}') {
        if ($lexer->peek()['kind'] == 'union') {
            $fieldlists[] = parse_union($lexer, $typenames);
        } else {
            $fieldlists[] = parse_struct_fieldlist($lexer, $typenames);
        }
    }
    expect($lexer, '}');
    return [
        'kind' => 'c_composite_type',
        'fieldlists' => $fieldlists
    ];
}

function parse_struct_fieldlist($lexer, $typenames)
{
    if ($lexer->follows('struct')) {
        throw new Exception("can't parse nested structs, please consider a typedef");
    }
    $type = parse_type($lexer);
    $forms = [];
    $forms[] = parse_form($lexer, $typenames);

    while ($lexer->follows(',')) {
        $lexer->get();
        $forms[] = parse_form($lexer, $typenames);
    }

    expect($lexer, ';');
    return [
        'kind' => 'c_struct_fieldlist',
        'type_name' => $type,
        'forms' => $forms
    ];
}

function parse_union($lexer, $typenames)
{
    $fields = [];
    expect($lexer, 'union');
    expect($lexer, '{');
    while (!$lexer->follows('}')) {
        $type = parse_type($lexer);
        $form = parse_form($lexer, $typenames);
        expect($lexer, ';');
        $fields[] = [
            'type_name' => $type,
            'form' => $form
        ];
    }
    expect($lexer, '}');
    $form = parse_form($lexer, $typenames);
    expect($lexer, ';');
    return [
        'kind' => 'c_union',
        'form' => $form,
        'fields' => $fields,
    ];
}

function parse_enum($lexer, $pub)
{
    return call_rust('parse_enum', $lexer, $pub);
}

function parse_for($lexer, $typenames)
{
    $init = null;
    $condition = null;
    $action = null;
    $body = null;

    expect($lexer, 'for');
    expect($lexer, '(');

    if ($lexer->peek()['kind'] == 'word' && is_type($lexer->peek()['content'], $typenames)) {
        $type = parse_type($lexer);
        $name = parse_identifier($lexer);
        expect($lexer, '=');
        $value = parse_expression($lexer, $typenames, 0);
        $init = [
            'kind' => 'c_loop_counter_declaration',
            'type_name' => $type,
            'name' => $name,
            'value' => $value
        ];
    } else {
        $init = parse_expression($lexer, $typenames, 0);
    }

    expect($lexer, ';');
    $condition = parse_expression($lexer, $typenames, 0);
    expect($lexer, ';');
    $action = parse_expression($lexer, $typenames, 0);
    expect($lexer, ')');
    $body = parse_body($lexer, $typenames);
    return [
        'kind' => 'c_for',
        'init' => $init,
        'condition' => $condition,
        'action' => $action,
        'body' => $body
    ];
}

function parse_type($lexer, $comment = null)
{
    return call_rust('parse_type', $lexer, $comment);
}

function parse_function_parameter($lexer, $typenames)
{
    $forms = [];
    $type = parse_type($lexer);
    $forms[] = parse_form($lexer, $typenames);
    while (
        $lexer->follows(',')
        && $lexer->peek_n(1)['kind'] != '...'
        && $lexer->peek_n(1)['kind'] != 'const'
        && !($lexer->peek_n(1)['kind'] == 'word' && is_type($lexer->peek_n(1)['content'], $typenames))
    ) {
        $lexer->get();
        $forms[] = parse_form($lexer, $typenames);
    }
    return [
        'type_name' => $type,
        'forms' => $forms
    ];
}

function parse_function_declaration($lexer, $pub, $type, $form, $typenames)
{
    $parameters = [];
    expect($lexer, '(');
    if (!$lexer->follows(')')) {
        $parameters[] = parse_function_parameter($lexer, $typenames);
        while ($lexer->follows(',')) {
            $lexer->get();
            if ($lexer->follows('...')) {
                $lexer->get();
                $parameters[] = [
                    'kind' => 'c_ellipsis'
                ];
                break;
            }
            $parameters[] = parse_function_parameter($lexer, $typenames);
        }
    }
    expect($lexer, ')');
    $body = parse_body($lexer, $typenames);
    return [
        'kind' => 'c_function_declaration',
        'is_pub' => $pub,
        'type_name' => $type,
        'form' => $form,
        'parameters' => [
            'kind' => 'c_function_parameters',
            'parameters' => $parameters
        ],
        'body' => $body
    ];
}

function parse_form($lexer, $typenames)
{
    // *argv[]
    $node = [
        'kind' => 'c_form',
        'str' => ''
    ];
    while ($lexer->follows('*')) {
        $node['str'] .= $lexer->get()['kind'];
    }
    $node['str'] .= expect($lexer, 'word')['content'];

    while ($lexer->follows('[')) {
        $node['str'] .= $lexer->get()['kind'];
        while ($lexer->more() && $lexer->peek()['kind'] != ']') {
            $expr = parse_expression($lexer, $typenames, 0);
            $node['str'] .= format_node($expr);
        }
        expect($lexer, ']');
        $node['str'] .= ']';
    }
    return $node;
}

function parse_switch($lexer, $typenames)
{
    expect($lexer, 'switch');
    expect($lexer, '(');
    $value = parse_expression($lexer, $typenames, 0);
    $cases = [];
    $default = null;
    expect($lexer, ')');
    expect($lexer, '{');
    while ($lexer->follows('case')) {
        expect($lexer, 'case');
        $case_value = null;
        if ($lexer->follows('word')) {
            $case_value = parse_identifier($lexer);
        } else {
            $case_value = parse_literal($lexer);
        }
        expect($lexer, ':');
        $until = ['case', 'break', 'default', '}'];
        $statements = [];
        while ($lexer->more() && !in_array($lexer->peek()['kind'], $until)) {
            $statements[] = parse_statement($lexer, $typenames);
        }
        $cases[] = [
            'value' => $case_value,
            'statements' => $statements
        ];
    }
    if ($lexer->follows('default')) {
        $def = [];
        expect($lexer, 'default');
        expect($lexer, ':');
        while ($lexer->more() && $lexer->peek()['kind'] != '}') {
            $def[] = parse_statement($lexer, $typenames);
        }
        $default = $def;
    }
    expect($lexer, '}');
    return [
        'kind' => 'c_switch',
        'value' => $value,
        'cases' => $cases,
        'default' => $default
    ];
}

function parse_literal($lexer)
{
    return call_rust("parse_literal", $lexer);
}

function parse_identifier($lexer)
{
    return call_rust('parse_identifier', $lexer);
}

function parse_if($lexer, $typenames)
{
    $condition = null;
    $body = null;
    $else = null;
    expect($lexer, 'if', 'if statement');
    expect($lexer, '(', 'if statement');
    $condition = parse_expression($lexer, $typenames, 0);
    expect($lexer, ')', 'if statement');
    $body = parse_body($lexer, $typenames);
    if ($lexer->follows('else')) {
        $lexer->get();
        $else = parse_body($lexer, $typenames);
    }
    return [
        'kind' => 'c_if',
        'condition' => $condition,
        'body' => $body,
        'else' => $else
    ];
}

function parse_struct_literal($lexer, $typenames)
{
    $members = [];
    expect($lexer, '{', 'struct literal');
    while (true) {
        expect($lexer, '.', 'struct literal member');
        $member_name = parse_identifier($lexer);
        expect($lexer, '=', 'struct literal member');
        $member_value = parse_expression($lexer, $typenames, 0);
        $members[] = [
            'name' => $member_name,
            'value' => $member_value
        ];
        if ($lexer->follows(',')) {
            $lexer->get();
            continue;
        } else {
            break;
        }
    }
    expect($lexer, '}', 'struct literal');
    return [
        'kind' => 'c_struct_literal',
        'members' => $members
    ];
}

function parse_anonymous_typeform($lexer)
{
    return call_rust('parse_anonymous_typeform', $lexer);
}

function parse_variable_declaration($lexer, $typenames)
{
    $type = parse_type($lexer);

    $forms = [parse_form($lexer, $typenames)];
    expect($lexer, '=', 'variable declaration');
    $values = [parse_expression($lexer, $typenames, 0)];

    while ($lexer->follows(',')) {
        $lexer->get();
        $forms[] = parse_form($lexer, $typenames);
        expect($lexer, '=', 'variable declaration');
        $values[] = parse_expression($lexer, $typenames, 0);
    }

    expect($lexer, ';');
    return [
        'kind' => 'c_variable_declaration',
        'type_name' => $type,
        'forms' => $forms,
        'values' => $values
    ];
}

function parse_return($lexer, $typenames)
{
    expect($lexer, 'return');
    if ($lexer->peek()['kind'] == ';') {
        $lexer->get();
        return [
            'kind' => 'c_return',
            'expression' => null
        ];
    }
    $expression = parse_expression($lexer, $typenames, 0);
    expect($lexer, ';');
    return [
        'kind' => 'c_return',
        'expression' => $expression
    ];
}

function parse_while($lexer, $typenames)
{
    expect($lexer, 'while');
    expect($lexer, '(');
    $condition = parse_expression($lexer, $typenames, 0);
    expect($lexer, ')');
    $body = parse_body($lexer, $typenames);
    return [
        'kind' => 'c_while',
        'condition' => $condition,
        'body' => $body
    ];
}

function parse_sizeof($lexer, $typenames)
{
    return call_rust('parse_sizeof', $lexer, $typenames);
}

function parse_function_call($lexer, $typenames, $function_name)
{
    $arguments = [];
    expect($lexer, '(');
    if ($lexer->more() && $lexer->peek()['kind'] != ')') {
        $arguments[] = parse_expression($lexer, $typenames, 0);
        while ($lexer->follows(',')) {
            $lexer->get();
            $arguments[] = parse_expression($lexer, $typenames, 0);
        }
    }
    expect($lexer, ')');
    return [
        'kind' => 'c_function_call',
        'function' => $function_name,
        'arguments' => $arguments
    ];
}
