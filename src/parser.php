<?php

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
        case 'struct':
            return c_struct_definition::parse($lexer);
        case 'macro':
            return c_compat_macro::parse($lexer);
    }

    $pub = false;
    if ($lexer->follows('pub')) {
        $lexer->get();
        $pub = true;
    }
    if ($lexer->follows('enum')) {
        return c_enum::parse($lexer, $pub);
    }
    try {
        $type = c_type::parse($lexer);
    } catch (Exception $e) {
        throw new Exception("unexpected input (expecting function, variable, typedef, struct, enum)");
    }
    $form = c_form::parse($lexer);
    if ($lexer->peek()['kind'] == '(') {
        $parameters = c_function_parameters::parse($lexer);
        $body = c_body::parse($lexer);
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
        return new c_module_variable($type, $form, $value);
    }
    throw new Exception("unexpected input");
}

function parse_anonymous_parameters($lexer)
{
    $node = new c_anonymous_parameters;
    expect($lexer, '(', 'anonymous function parameters');
    if (!$lexer->follows(')')) {
        $node->forms[] = c_anonymous_typeform::parse($lexer);
        while ($lexer->follows(',')) {
            $lexer->get();
            $node->forms[] = c_anonymous_typeform::parse($lexer);
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
    $node = new c_typedef;

    expect($lexer, 'typedef');

    if ($lexer->follows('{')) {
        $node->type = c_composite_type::parse($lexer);
        $node->alias = c_identifier::parse($lexer);
        expect($lexer, ';', 'typedef');
        return $node;
    }

    $node->type = c_type::parse($lexer, 'typedef');
    while ($lexer->follows('*')) {
        $node->before .= $lexer->get()['kind'];
    }
    $node->alias = c_identifier::parse($lexer);

    if ($lexer->follows('(')) {
        $node->after .= parse_anonymous_parameters($lexer)->format();
    }

    if ($lexer->follows('[')) {
        $lexer->get();
        $node->after .= '[';
        $node->after .= expect($lexer, 'num')['content'];
        expect($lexer, ']');
        $node->after .= ']';
    }

    expect($lexer, ';', 'typedef');
    return $node;
}

function parse_statement($lexer)
{
    $next = $lexer->peek();
    if (
        ($next['kind'] == 'word' && is_type($next['content'], $lexer->typenames))
        || $next['kind'] == 'const'
    ) {
        return c_variable_declaration::parse($lexer);
    }

    switch ($next['kind']) {
        case 'if':
            return c_if::parse($lexer);
        case 'for':
            return c_for::parse($lexer);
        case 'while':
            return c_while::parse($lexer);
        case 'defer':
            return c_defer::parse($lexer);
        case 'return':
            return c_return::parse($lexer);
        case 'switch':
            return c_switch::parse($lexer);
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
        && $lexer->peek(1)['kind'] == 'word'
        && is_type($lexer->peek(1)['content'], $lexer->typenames)
    ) {
        expect($lexer, '(');
        $typeform = c_anonymous_typeform::parse($lexer);
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
        if ($lexer->peek(1)['kind'] == '.') {
            return c_struct_literal::parse($lexer);
        }
        return c_array_literal::parse($lexer);
    }

    if ($lexer->peek()['kind'] == 'sizeof') {
        return c_sizeof::parse($lexer);
    }

    if (is_prefix_op($lexer->peek()['kind'])) {
        $op = $lexer->get()['kind'];
        return new c_prefix_operator($op, parse_expression($lexer, operator_strength('prefix')));
    }

    if ($lexer->peek()['kind'] == 'word') {
        $result = c_identifier::parse($lexer);
    } else {
        $result = c_literal::parse($lexer);
    }

    while ($lexer->more()) {
        if ($lexer->peek()['kind'] == '(') {
            $args = c_function_arguments::parse($lexer);
            $result = new c_function_call($result, $args);
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
