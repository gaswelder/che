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

function format_expression($expression)
{
    $s = '';
    foreach ($expression as $atom) {
        if (is_string($atom)) {
            $s .= ' ' . $atom . ' ';
            continue;
        }
        if (get_class($atom) == 'token') {
            $s .= ' ' . $atom->type . ' ';
            continue;
        }
        $s .= $atom->format();
    }
    return $s;
}

function is_op($token)
{
    $ops = ['+', '-', '<', '>', '*', '/', '!=', '->', '.'];

    return in_array($token->type, $ops);
}

function parse_expression($lexer)
{
    $parts = [
        parse_atom($lexer)
    ];

    while ($lexer->more() && is_op($lexer->peek())) {
        $parts[] = $lexer->get();
        $parts[] = parse_atom($lexer);
    }

    return $parts;
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
        if ($lexer->peek(1)->type == '=') {
            return c_assignment::parse($lexer);
        }
        if ($lexer->peek(1)->type == '[') {
            return c_index::parse($lexer);
        }

        $var = c_variable::parse($lexer);
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
        return '--' . $this->operand->format();
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
        return '++(not postfix?)' . $this->operand->format();
    }
}


class c_literal
{
    private $value;
    private $type;

    static function parse($lexer)
    {
        $types = ['string', 'num'];
        $self = new self;
        foreach ($types as $type) {
            if ($lexer->peek()->type != $type) {
                continue;
            }
            $self->value = $lexer->get()->content;
            $self->type = $type;
            return $self;
        }
        throw new Exception("literal expected");
    }

    function format()
    {
        if ($this->type == 'string') {
            return '"' . $this->value . '"';
        }
        return $this->value;
    }
}


class c_prefix_operator
{
    private $operand;
    private $type;

    static function parse($lexer)
    {
        $self = new self;
        $ops = ['&', '*', '!'];
        foreach ($ops as $op) {
            if ($lexer->peek()->type == $op) {
                $lexer->get();
                $self->type = $op;
                $self->operand = c_identifier::parse($lexer);
                return $self;
            }
        }
        throw new Exception("operator expected");
    }

    function format()
    {
        return $this->type . $this->operand->format();
    }
}

class c_variable
{
    private $name;

    static function parse($lexer)
    {
        $self = new self;
        $self->name = expect($lexer, 'word')->content;
        return $self;
    }

    function format()
    {
        return $this->name;
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