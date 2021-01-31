<?php

function parse_module($lexer)
{
    $m = new c_module;
    while ($lexer->more()) {
        $m->elements[] = parse_module_element($lexer);
    }
    return $m;
}

function parse_module_element($lexer)
{
    switch ($lexer->peek()['kind']) {
        case 'import':
            $import = parse_import($lexer);
            $m = resolve_import($import);
            $lexer->typenames = array_merge($lexer->typenames, $m->types());
            return $import;
        case 'typedef':
            return parse_typedef($lexer);
        case 'macro':
            return parse_compat_macro($lexer);
    }

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
    $form = parse_form($lexer);
    if ($lexer->peek()['kind'] == '(') {
        $parameters = parse_function_parameters($lexer);
        $body = parse_body($lexer);
        return new c_function_declaration($pub, $type, $form, $parameters, $body);
    }

    if ($lexer->peek()['kind'] != '=') {
        throw new Exception("module variable: '=' expected");
    }

    if ($lexer->peek()['kind'] == '=') {
        if ($pub) {
            throw new Exception("module variables can't be exported");
        }
        $lexer->get();
        $value = parse_expression($lexer);
        expect($lexer, ';', 'module variable declaration');
        return [
            'kind' => 'c_module_variable',
            'type' => $type,
            'form' => $form,
            'value' => $value
        ];
    }
    throw new Exception("unexpected input");
}

function parse_anonymous_parameters($lexer)
{
    $node = new c_anonymous_parameters;
    expect($lexer, '(', 'anonymous function parameters');
    if (!$lexer->follows(')')) {
        $node->forms[] = parse_anonymous_typeform($lexer);
        while ($lexer->follows(',')) {
            $lexer->get();
            $node->forms[] = parse_anonymous_typeform($lexer);
        }
    }
    expect($lexer, ')', 'anonymous function parameters');
    return $node;
}

function parse_import($lexer)
{
    expect($lexer, 'import');
    $tok = expect($lexer, 'string');
    // expect($lexer, ';');
    $node = new c_import;
    $node->path = $tok['content'];
    return $node;
}

function parse_typedef($lexer)
{
    expect($lexer, 'typedef');

    $type = null;
    $alias = null;
    $before = null;
    $after = null;

    if ($lexer->follows('{')) {
        $type = parse_composite_type($lexer);
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
        'type' => $type,
        'before' => $before,
        'after' => $after,
        'alias' => $alias
    ];
}

function parse_statement($lexer)
{
    $next = $lexer->peek();
    if (
        ($next['kind'] == 'word' && is_type($next['content'], $lexer->typenames))
        || $next['kind'] == 'const'
    ) {
        return parse_variable_declaration($lexer);
    }

    switch ($next['kind']) {
        case 'if':
            return parse_if($lexer);
        case 'for':
            return parse_for($lexer);
        case 'while':
            return parse_while($lexer);
        case 'defer':
            return parse_defer($lexer);
        case 'return':
            return parse_return($lexer);
        case 'switch':
            return parse_switch($lexer);
    }

    $expr = parse_expression($lexer);
    expect($lexer, ';', 'parsing statement');
    return $expr;
}

function parse_expression($lexer, $current_strength = 0)
{
    $result = parse_atom($lexer);
    while (is_op($lexer->peek()['kind'])) {
        // If the operator is not stronger that our current level,
        // yield the result.
        if (operator_strength($lexer->peek()['kind']) <= $current_strength) {
            return $result;
        }
        $op = $lexer->get()['kind'];
        $next = parse_expression($lexer, operator_strength($op));
        $result = new c_binary_op($op, $result, $next);
    }
    return $result;
}

function parse_atom($lexer)
{
    $nono = ['case', 'default', 'if', 'else', 'for', 'while', 'switch'];
    if (in_array($lexer->peek()['kind'], $nono)) {
        throw new Exception("expression: unexpected keyword '" . $lexer->peek()['kind'] . "'");
    }

    if (
        $lexer->peek()['kind'] == '('
        && $lexer->peek_n(1)['kind'] == 'word'
        && is_type($lexer->peek_n(1)['content'], $lexer->typenames)
    ) {
        expect($lexer, '(');
        $typeform = parse_anonymous_typeform($lexer);
        expect($lexer, ')', 'typecast');
        return new c_cast($typeform, parse_expression($lexer));
    }

    if ($lexer->peek()['kind'] == '(') {
        $lexer->get();
        $expr = parse_expression($lexer);
        expect($lexer, ')');
        return $expr;
    }

    if ($lexer->peek()['kind'] == '{') {
        if ($lexer->peek_n(1)['kind'] == '.') {
            return parse_struct_literal($lexer);
        }
        return parse_array_literal($lexer);
    }

    if ($lexer->peek()['kind'] == 'sizeof') {
        return parse_sizeof($lexer);
    }

    if (is_prefix_op($lexer->peek()['kind'])) {
        $op = $lexer->get()['kind'];
        return new c_prefix_operator($op, parse_expression($lexer, operator_strength('prefix')));
    }

    if ($lexer->peek()['kind'] == 'word') {
        $result = parse_identifier($lexer);
    } else {
        $result = parse_literal($lexer);
    }

    while ($lexer->more()) {
        if ($lexer->peek()['kind'] == '(') {
            $arguments = new c_function_arguments;
            expect($lexer, '(');
            if ($lexer->more() && $lexer->peek()['kind'] != ')') {
                $arguments->arguments[] = parse_expression($lexer);
                while ($lexer->follows(',')) {
                    $lexer->get();
                    $arguments->arguments[] = parse_expression($lexer);
                }
            }
            expect($lexer, ')');
            $result = new c_function_call($result, $arguments);
            continue;
        }
        if ($lexer->peek()['kind'] == '[') {
            expect($lexer, '[', 'array index');
            $index = parse_expression($lexer);
            expect($lexer, ']', 'array index');
            $result = new c_array_index($result, $index);
            continue;
        }

        if (is_postfix_op($lexer->peek()['kind'])) {
            $op = $lexer->get()['kind'];
            $result = new c_postfix_operator($result, $op);
            continue;
        }
        break;
    }

    return $result;
}

function with_comment($comment, $message)
{
    if ($comment) {
        return $comment . ': ' . $message;
    }
    return $message;
}

function expect($lexer, $type, $comment = null)
{
    if (!$lexer->more()) {
        throw new Exception(with_comment($comment, "expected '$type', got end of file"));
    }
    $next = $lexer->peek();
    if ($next['kind'] != $type) {
        throw new Exception(with_comment($comment, "expected '$type', got $next at $next[pos]"));
    }
    return $lexer->get();
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

function parse_array_literal_member($lexer)
{
    if ($lexer->follows('{')) {
        return parse_array_literal($lexer);
    }
    if ($lexer->follows('word')) {
        return parse_identifier($lexer);
    }
    return parse_literal($lexer);
}

function parse_array_literal_entry($lexer)
{
    $node = new c_array_literal_entry;
    if ($lexer->follows('[')) {
        $lexer->get();
        if ($lexer->follows('word')) {
            $node->index = parse_identifier($lexer);
        } else {
            $node->index = parse_literal($lexer);
        }
        expect($lexer, ']', 'array literal entry');
        expect($lexer, '=', 'array literal entry');
    }
    $node->value = parse_array_literal_member($lexer);
    return $node;
}

function parse_array_literal($lexer)
{
    $node = new c_array_literal;
    expect($lexer, '{', 'array literal');
    if (!$lexer->follows('}')) {
        $node->values[] = parse_array_literal_entry($lexer);
        while ($lexer->follows(',')) {
            $lexer->get();
            $node->values[] = parse_array_literal_entry($lexer);
        }
    }
    expect($lexer, '}', 'array literal');
    return $node;
}

function parse_body($lexer)
{
    $node = new c_body;
    if ($lexer->follows('{')) {
        expect($lexer, '{');
        while (!$lexer->follows('}')) {
            $node->statements[] = parse_statement($lexer);
        }
        expect($lexer, '}');
    } else {
        $node->statements[] = parse_statement($lexer);
    }
    return $node;
}

function parse_compat_macro($lexer)
{
    $node = new c_compat_macro;
    $node->content = expect($lexer, 'macro')['content'];
    return $node;
}

function parse_composite_type($lexer)
{
    $node = new c_composite_type;
    expect($lexer, '{', 'struct type definition');
    while ($lexer->more() && $lexer->peek()['kind'] != '}') {
        if ($lexer->peek()['kind'] == 'union') {
            $node->fieldlists[] = parse_union($lexer);
        } else {
            $node->fieldlists[] = parse_struct_fieldlist($lexer);
        }
    }
    expect($lexer, '}');
    return $node;
}

function parse_struct_fieldlist($lexer)
{
    $node = new c_struct_fieldlist;
    if ($lexer->follows('struct')) {
        throw new Exception("can't parse nested structs, please consider a typedef");
    }
    $node->type = parse_type($lexer);
    $node->forms[] = parse_form($lexer);

    while ($lexer->follows(',')) {
        $lexer->get();
        $node->forms[] = parse_form($lexer);
    }

    expect($lexer, ';');
    return $node;
}

function parse_defer($lexer)
{
    expect($lexer, 'defer');
    $node = new c_defer;
    $node->expression = parse_expression($lexer);
    expect($lexer, ';');
    return $node;
}

function parse_union($lexer)
{
    $fields = [];
    expect($lexer, 'union');
    expect($lexer, '{');
    while (!$lexer->follows('}')) {
        $type = parse_type($lexer);
        $form = parse_form($lexer);
        expect($lexer, ';');
        $fields[] = [
            'type' => $type,
            'form' => $form
        ];
    }
    expect($lexer, '}');
    $form = parse_form($lexer);
    expect($lexer, ';');
    return [
        'kind' => 'c_union',
        'form' => $form,
        'fields' => $fields,
    ];
}

function parse_enum($lexer, $pub)
{
    $node = new c_enum;
    $node->pub = $pub;
    expect($lexer, 'enum', 'enum definition');
    expect($lexer, '{', 'enum definition');
    while (true) {
        $member = new c_enum_member;
        $member->id = parse_identifier($lexer);
        if ($lexer->follows('=')) {
            $lexer->get();
            $member->value = parse_literal($lexer);
        }
        $node->members[] = $member;
        if ($lexer->follows(',')) {
            $lexer->get();
            continue;
        } else {
            break;
        }
    }
    expect($lexer, '}', 'enum definition');
    expect($lexer, ';', 'enum definition');
    return $node;
}

function parse_for($lexer)
{
    $node = new c_for;
    expect($lexer, 'for');
    expect($lexer, '(');

    if ($lexer->peek()['kind'] == 'word' && is_type($lexer->peek()['content'], $lexer->typenames)) {
        $type = parse_type($lexer);
        $name = parse_identifier($lexer);
        expect($lexer, '=');
        $value = parse_expression($lexer);
        $node->init = [
            'kind' => 'c_loop_counter_declaration',
            'type' => $type,
            'name' => $name,
            'value' => $value
        ];
    } else {
        $node->init = parse_expression($lexer);
    }

    expect($lexer, ';');
    $node->condition = parse_expression($lexer);
    expect($lexer, ';');
    $node->action = parse_expression($lexer);
    expect($lexer, ')');
    $node->body = parse_body($lexer);
    return $node;
}

function parse_type($lexer, $comment = null)
{
    $const = false;
    $type = '';

    if ($lexer->follows('const')) {
        $lexer->get();
        $const = true;
    }

    if ($lexer->follows('struct')) {
        $lexer->get();
        $name = expect($lexer, 'word')['content'];
        $type = 'struct ' . $name;
    } else {
        $tok = expect($lexer, 'word', $comment);
        $type = $tok['content'];
    }

    return [
        'kind' => 'c_type',
        'const' => $const,
        'type' => $type
    ];
}

function parse_function_parameter($lexer)
{
    $node = new c_function_parameter;
    $node->type = parse_type($lexer);
    $node->forms[] = parse_form($lexer);
    while (
        $lexer->follows(',')
        && $lexer->peek_n(1)['kind'] != '...'
        && $lexer->peek_n(1)['kind'] != 'const'
        && !($lexer->peek_n(1)['kind'] == 'word' && is_type($lexer->peek_n(1)['content'], $lexer->typenames))
    ) {
        $lexer->get();
        $node->forms[] = parse_form($lexer);
    }
    return $node;
}

function parse_function_parameters($lexer)
{
    $node = new c_function_parameters;

    expect($lexer, '(');
    if (!$lexer->follows(')')) {
        $node->parameters[] = parse_function_parameter($lexer);
        while ($lexer->follows(',')) {
            $lexer->get();
            if ($lexer->follows('...')) {
                $lexer->get();
                $node->parameters[] = new c_ellipsis();
                break;
            }
            $node->parameters[] = parse_function_parameter($lexer);
        }
    }
    expect($lexer, ')');
    return $node;
}

function parse_form($lexer)
{
    // *argv[]
    $node = new c_form;
    while ($lexer->follows('*')) {
        $node->str .= $lexer->get()['kind'];
    }
    $node->str .= expect($lexer, 'word')['content'];

    while ($lexer->follows('[')) {
        $node->str .= $lexer->get()['kind'];
        while ($lexer->more() && $lexer->peek()['kind'] != ']') {
            $expr = parse_expression($lexer);
            $node->str .= format_node($expr);
        }
        expect($lexer, ']');
        $node->str .= ']';
    }
    return $node;
}

function parse_switch($lexer)
{
    expect($lexer, 'switch');
    expect($lexer, '(');
    $value = parse_expression($lexer);
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
            $statements[] = parse_statement($lexer);
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
            $def[] = parse_statement($lexer);
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
    $types = ['string', 'num', 'char'];
    $node = new c_literal;
    foreach ($types as $type) {
        if ($lexer->peek()['kind'] != $type) {
            continue;
        }
        $node->value = $lexer->get()['content'];
        $node->type = $type;
        return $node;
    }
    if ($lexer->peek()['kind'] == 'word' && $lexer->peek()['content'] == 'NULL') {
        $lexer->get();
        $node->type = 'null';
        $node->value = 'NULL';
        return $node;
    }
    throw new Exception("literal expected, got " . $lexer->peek());
}

function parse_identifier($lexer)
{
    $tok = expect($lexer, 'word');
    $node = new c_identifier;
    $node->name = $tok['content'];
    return $node;
}

function parse_if($lexer)
{
    $node = new c_if;
    expect($lexer, 'if', 'if statement');
    expect($lexer, '(', 'if statement');
    $node->condition = parse_expression($lexer);
    expect($lexer, ')', 'if statement');
    $node->body = parse_body($lexer);
    if ($lexer->follows('else')) {
        $lexer->get();
        $node->else = parse_body($lexer);
    }
    return $node;
}

function parse_struct_literal($lexer)
{
    $members = [];
    expect($lexer, '{', 'struct literal');
    while (true) {
        expect($lexer, '.', 'struct literal member');
        $member_name = parse_identifier($lexer);
        expect($lexer, '=', 'struct literal member');
        $member_value = parse_expression($lexer);
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
    $node = new c_anonymous_typeform;
    $node->type = parse_type($lexer);
    while ($lexer->follows('*')) {
        $node->ops[] = $lexer->get()['kind'];
    }
    return $node;
}

function parse_variable_declaration($lexer)
{
    $type = parse_type($lexer);

    $forms = [parse_form($lexer)];
    expect($lexer, '=', 'variable declaration');
    $values = [parse_expression($lexer)];

    while ($lexer->follows(',')) {
        $lexer->get();
        $forms[] = parse_form($lexer);
        expect($lexer, '=', 'variable declaration');
        $values[] = parse_expression($lexer);
    }

    expect($lexer, ';');
    return [
        'kind' => 'c_variable_declaration',
        'type' => $type,
        'forms' => $forms,
        'values' => $values
    ];
}

function parse_return($lexer)
{
    expect($lexer, 'return');
    if ($lexer->peek()['kind'] == ';') {
        $lexer->get();
        return [
            'kind' => 'c_return',
            'expression' => null
        ];
    }
    $expression = parse_expression($lexer);
    expect($lexer, ';');
    return [
        'kind' => 'c_return',
        'expression' => $expression
    ];
}

function parse_while($lexer)
{
    expect($lexer, 'while');
    expect($lexer, '(');
    $condition = parse_expression($lexer);
    expect($lexer, ')');
    $body = parse_body($lexer);
    return [
        'kind' => 'c_while',
        'condition' => $condition,
        'body' => $body
    ];
}

function parse_sizeof($lexer)
{
    expect($lexer, 'sizeof');
    expect($lexer, '(');
    if ($lexer->peek()['kind'] == 'word' && is_type($lexer->peek()['content'], $lexer->typenames)) {
        $argument = parse_type($lexer);
    } else {
        $argument = parse_expression($lexer);
    }
    expect($lexer, ')');
    return [
        'kind' => 'c_sizeof',
        'argument' => $argument
    ];
}
