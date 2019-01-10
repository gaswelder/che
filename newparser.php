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
// $path = 'test/expressions.c';
$lexer = new lexer($path);
$lexer->typenames = typenames($path);
$m = parse_program($lexer);
var_dump($m);
if ($m) echo $m->format();

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
    $ops = ['!', '--', '++'];
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
    $next = $lexer->peek();

    switch ($next->type) {
        case 'string':
        case 'num':
            return c_literal::parse($lexer);
        case '&':
        case '*':
        case '!':
        case '--':
        case '++':
            return c_prefix_operator::parse($lexer);
        case '{':
            return c_array_literal::parse($lexer);
        case 'sizeof':
            return c_sizeof::parse($lexer);
    }

    if ($next->type == '(' && is_type($lexer->peek(1), $lexer->typenames)) {
        return c_cast::parse($lexer);
    }

    if ($next->type == 'word') {
        if ($lexer->peek(1)->type == '(') {
            return c_function_call::parse($lexer);
        }
        if ($lexer->peek(1)->type == '[') {
            return c_index::parse($lexer);
        }

        $var = c_identifier::parse($lexer);
        if ($lexer->peek()->type == '--') {
            $lexer->get();
            return new c_op_decrement($var);
        }
        if ($lexer->peek()->type == '++') {
            $lexer->get();
            return new c_op_increment($var);
        }
        return $var;
    }

    var_dump($lexer->peek());
    throw new Exception("unknown expression");
}

function indent($text)
{
    if (substr($text, -1) == "\n") {
        return indent(substr($text, 0, -1)) . "\n";
    }
    return "\t" . str_replace("\n", "\n\t", $text);
}

class c_index
{
    private $operand;
    private $index;

    static function parse($lexer)
    {
        $self = new self;
        $self->operand = c_identifier::parse($lexer);
        expect($lexer, '[');
        $self->index = expect($lexer, 'num')->content;
        expect($lexer, ']');
        return $self;
    }

    function format()
    {
        return $this->operand->format() . "[$this->index]";
    }
}

class c_op_decrement
{
    private $operand;

    function __construct($operand)
    {
        $this->operand = $operand;
    }

    function format()
    {
        return $this->operand->format() . '--';
    }
}

class c_op_increment
{
    private $operand;

    function __construct($operand)
    {
        $this->operand = $operand;
    }

    function format()
    {
        return $this->operand->format() . '++';
    }
}

function expect($lexer, $type)
{
    if (!$lexer->more()) {
        throw new Exception("expected '$type', got end of file");
    }
    $next = $lexer->peek();
    if ($next->type != $type) {
        throw new Exception("expected '$type', got ('$next->type', '$next->content') at $next->pos");
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
    expect($lexer, ';');
    return $expr;
}