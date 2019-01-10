<?php

require 'mc/parser/lexer.php';
require 'mc/parser/token.php';
require 'mc/debug.php';
require 'mc/module/package_file.php';

function typenames($path)
{
    $file = new package_file($path);
    return $file->typenames();
}

foreach (glob('mc/nodes/*.php') as $path) {
    require $path;
}

// $lexer = new lexer('prog/rand.c');
$path = 'prog/mp3cuespl.c';
// $path = 'test/types.c';
$path = 'test/expressions.c';

$lexer = new lexer($path);
$lexer->typenames = typenames($path);
$m = parse_program($lexer);
var_dump($m);
echo $m->format();
exit;

foreach (glob('test/*.c') as $path) {
    echo $path, "\n";
    $lexer = new lexer($path);
    $lexer->typenames = typenames($path);
    $m = parse_program($lexer);
    var_dump($m);
    if (!$m) break;
    echo $m->format();
}

function parse_program($lexer)
{
    // return c_module::parse($lexer);
    try {
        return c_module::parse($lexer);
    } catch (Exception $e) {
        $next = $lexer->peek();
        echo $e->getMessage(), ' at ' . $lexer->peek()->pos, ": $next ...\n";
    }
}

function is_op($token)
{
    $ops = [
        '+', '-', '*', '/', '=', '|', '&', '~', '^', '<', '>', '?',
        ':', '%', '+=', '-=', '*=', '/=', '%=', '&=', '^=', '|=', '++',
        '--', '->', '.', '>>', '<<', '<=', '>=', '&&', '||', '==', '!=',
        '<<=', '>>='
    ];

    return in_array($token->type, $ops);
}

function is_prefix_op($token)
{
    $ops = ['!', '--', '++', '*', '~', '&', '-'];
    return in_array($token->type, $ops);
}

function is_postfix_op($token)
{
    $ops = ['++', '--', '(', '['];
    return in_array($token->type, $ops);
}

function operator_strength($op)
{
    $map = [
        [','],
        ['=', '+=', '-=', '*=', '/=', '%=', '<<=', '>>=', '&=', '^=', '|='],
        ['||'],
        ['&&'],
        ['|'],
        ['^'],
        ['&'],
        ['!=', '=='],
        ['>', '<', '>=', '<='],
        ['<<', '>>'],
        ['+', '-'],
        ['*', '/', '%'],
        ['->', '.']
    ];
    foreach ($map as $i => $ops) {
        if (in_array($op, $ops)) {
            return $i + 1;
        }
    }
    throw new Exception("unknown operator: '$op'");
}

function parse_expression($lexer, $level = 0)
{
    $result = parse_atom($lexer);
    while (is_op($lexer->peek())) {
        // If the operator is not stronger that our current level,
        // yield the result.
        if (operator_strength($lexer->peek()->type) <= $level) {
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
    switch ($lexer->peek()->type) {
        case '{':
            return c_array_literal::parse($lexer);
        case 'sizeof':
            return c_sizeof::parse($lexer);
    }

    $pre = [];
    while ($lexer->more()) {
        if (is_prefix_op($lexer->peek())) {
            $pre[] = $lexer->get()->type;
            continue;
        }
        if ($lexer->peek()->type == '(' && is_type($lexer->peek(1), $lexer->typenames)) {
            expect($lexer, '(');
            $pre[] = c_type::parse($lexer);
            expect($lexer, ')');
            continue;
        }
        break;
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
            $index = expect($lexer, 'num', 'array index')->content;
            expect($lexer, ']', 'array index');
            $result = new c_array_index($result, $index);
            continue;
        }

        if (is_postfix_op($lexer->peek())) {
            $op = $lexer->get()->type;
            $result = new c_postfix_operator($result, $op);
            continue;
        }
        break;
    }

    while (!empty($pre)) {
        $op = array_pop($pre);
        if ($op instanceof c_type) {
            $result = new c_cast($op, $result);
        } else {
            $result = new c_prefix_operator($op, $result);
        }
    }

    return $result;
}

function indent($text)
{
    if (substr($text, -1) == "\n") {
        return indent(substr($text, 0, -1)) . "\n";
    }
    return "\t" . str_replace("\n", "\n\t", $text);
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


function is_type($token, $typenames)
{
    $types = [
        'struct',
        'enum',
        'union',
        'void',
        'char',
        'short',
        'int',
        'long',
        'float',
        'double',
        'unsigned',
        'bool',
        'va_list',
        'FILE',
        'ptrdiff_t',
        'size_t',
        'wchar_t',
        'int8_t',
        'int16_t',
        'int32_t',
        'int64_t',
        'uint8_t',
        'uint16_t',
        'uint32_t',
        'uint64_t',
        'clock_t',
        'time_t',
        'fd_set',
        'socklen_t',
        'ssize_t'
    ];
    if ($token->type != 'word') {
        return false;
    }
    $name = $token->content;

    return in_array($name, $types) || in_array($name, $typenames) || substr($name, -2) == '_t';
}

function parse_statement($lexer)
{
    if (is_type($lexer->peek(), $lexer->typenames) || $lexer->peek()->type == 'const') {
        return c_variable_declaration::parse($lexer);
    }

    switch ($lexer->peek()->type) {
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
    }

    $expr = parse_expression($lexer);
    expect($lexer, ';', 'parsing statement');
    return $expr;
}