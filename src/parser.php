<?php

function parse_program($lexer, $filename)
{
    // return c_module::parse($lexer);
    try {
        return c_module::parse($lexer);
    } catch (Exception $e) {
        $next = $lexer->peek();
        $where = "$filename:" . $lexer->peek()->pos;
        $what = $e->getMessage();
        echo "$where: $what: $next...\n";
    }
}

function parse_module_element($lexer)
{
    switch ($lexer->peek()->type) {
        case 'import':
            $import = c_import::parse($lexer);
            $m = resolve_import($import);
            $lexer->typenames = array_merge($lexer->typenames, $m->types());
            return $import;
        case 'typedef':
            return c_typedef::parse($lexer);
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
    if ($lexer->peek()->type == '(') {
        $parameters = c_function_parameters::parse($lexer);
        $body = c_body::parse($lexer);
        return new c_function_declaration($pub, $type, $form, $parameters, $body);
    }

    if ($lexer->peek()->type != '=') {
        throw new Exception("module variable: '=' expected");
    }

    if ($lexer->peek()->type == '=') {
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

function parse_statement($lexer)
{
    $next = $lexer->peek();
    if (
        ($next->type == 'word' && is_type($next->content, $lexer->typenames))
        || $next->type == 'const'
    ) {
        return c_variable_declaration::parse($lexer);
    }

    switch ($next->type) {
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
    while (is_op($lexer->peek()->type)) {
        // If the operator is not stronger that our current level,
        // yield the result.
        if (operator_strength($lexer->peek()->type) <= $current_strength) {
            return $result;
        }
        $op = $lexer->get()->type;
        $next = parse_expression($lexer, operator_strength($op));
        $result = new c_binary_op($op, $result, $next);
    }
    return $result;
}

function parse_atom($lexer)
{
    $nono = ['case', 'default', 'if', 'else', 'for', 'while', 'switch'];
    if (in_array($lexer->peek()->type, $nono)) {
        throw new Exception("expression: unexpected keyword '" . $lexer->peek()->type . "'");
    }

    if (
        $lexer->peek()->type == '('
        && $lexer->peek(1)->type == 'word'
        && is_type($lexer->peek(1)->content, $lexer->typenames)
    ) {
        expect($lexer, '(');
        $typeform = c_anonymous_typeform::parse($lexer);
        expect($lexer, ')', 'typecast');
        return new c_cast($typeform, parse_expression($lexer));
    }

    if ($lexer->peek()->type == '(') {
        $lexer->get();
        $expr = parse_expression($lexer);
        expect($lexer, ')');
        return $expr;
    }

    if ($lexer->peek()->type == '{') {
        if ($lexer->peek(1)->type == '.') {
            return c_struct_literal::parse($lexer);
        }
        return c_array_literal::parse($lexer);
    }

    if ($lexer->peek()->type == 'sizeof') {
        return c_sizeof::parse($lexer);
    }

    if (is_prefix_op($lexer->peek()->type)) {
        $op = $lexer->get()->type;
        return new c_prefix_operator($op, parse_expression($lexer, operator_strength('prefix')));
    }

    if ($lexer->peek()->type == 'word') {
        $result = c_identifier::parse($lexer);
    } else {
        $result = c_literal::parse($lexer);
    }

    while ($lexer->more()) {
        if ($lexer->peek()->type == '(') {
            $args = c_function_arguments::parse($lexer);
            $result = new c_function_call($result, $args);
            continue;
        }
        if ($lexer->peek()->type == '[') {
            expect($lexer, '[', 'array index');
            $index = parse_expression($lexer);
            expect($lexer, ']', 'array index');
            $result = new c_array_index($result, $index);
            continue;
        }

        if (is_postfix_op($lexer->peek()->type)) {
            $op = $lexer->get()->type;
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
    if ($next->type != $type) {
        throw new Exception(with_comment($comment, "expected '$type', got $next at $next->pos"));
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
