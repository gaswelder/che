<?php

require 'mc/parser/lexer.php';
require 'mc/parser/token.php';
require 'mc/debug.php';
require 'mc/module/package_file.php';
foreach (glob('mc/nodes/*.php') as $path) {
    require $path;
}

// $path = 'test/forms.c';
// $m = parse_path($path);
// var_dump($m);
// echo $m->format();
// exit;



// $m = parse_path($path);
// // echo json_encode($m->json(), JSON_PRETTY_PRINT);
// var_dump($m);
// echo $m->format();
// // var_dump($m->imports());
// echo dep_tree($m);
// exit;


foreach (glob('test/*.c') as $path) {
    echo $path, "\n";
    $m = parse_path($path);
    // echo $m->format();
    echo dep_tree($m);
}

function dep_tree(c_module $module)
{
    $s = '';
    $imps = $module->imports();
    foreach ($imps as $imp) {
        $s .= $imp->name() . "\n";
        $s .= indent(dep_tree(resolve_import($imp))) . "\n";
    }
    $s .= "\n";
    return indent($s);
}

function parse_path($module_path)
{
    // A module can be written as one .c file, or as multiple
    // .c files in a directory. Treat both cases as one by reducing
    // to a list of .c files to parse and merge.
    if (is_dir($module_path)) {
        $paths = glob("$module_path/*.c");
    } else {
        $paths = [$module_path];
    }

    // Preview all module files and collect what new types they define.
    $types = [];
    foreach ($paths as $path) {
        $types = array_merge($types, typenames($path));
    }

    // Parse each file separately using the gathered types information.
    $modules = array_map(function ($path) use ($types) {
        $lexer = new lexer($path);
        $lexer->typenames = $types;
        return parse_program($lexer, $path);
    }, $paths);

    // Merge all partial modules into one.
    $result = array_reduce($modules, function ($a, $b) {
        if (!$a) return $b;
        return $a->merge($b);
    }, null);

    return $result;
}


function resolve_import(c_import $import)
{
    $name = $import->name();
    $paths = [
        'lib/' . $name . '.c',
        "lib/$name",
        // $name
    ];
    foreach ($paths as $path) {
        if (file_exists($path)) {
            return parse_path($path);
        }
    }
    throw new Exception("can't find module '$name'");
}



function typenames($path)
{
    $file = new package_file($path);
    return $file->typenames();
}

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
        case 'enum':
            return c_enum::parse($lexer);
        case 'macro':
            return c_compat_macro::parse($lexer);
    }

    $pub = false;
    if ($lexer->follows('pub')) {
        $lexer->get();
        $pub = true;
    }
    try {
        $type = c_type::parse($lexer);
    } catch (Exception $e) {
        throw new Exception("unexpected input (expecting function, variable, typedef, struct)");
    }
    $form = c_form::parse($lexer);
    if ($lexer->peek()->type == '(') {
        $parameters = c_function_parameters::parse($lexer);
        $body = c_body::parse($lexer);
        return new c_function_declaration($pub, $type, $form, $parameters, $body);
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

class c_struct_literal_member
{
    private $name;
    private $value;

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, '.', 'struct literal member');
        $self->name = c_identifier::parse($lexer, 'struct literal member');
        expect($lexer, '=', 'struct literal member');
        $self->value = parse_expression($lexer, 'struct literal member');
        return $self;
    }

    function format()
    {
        return '.' . $this->name->format() . ' = ' . $this->value->format();
    }
}

class c_struct_literal
{
    private $members = [];

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, '{', 'struct literal');
        $self->members[] = c_struct_literal_member::parse($lexer);
        while ($lexer->follows(',')) {
            $lexer->get();
            $self->members[] = c_struct_literal_member::parse($lexer);
        }
        expect($lexer, '}', 'struct literal');
        return $self;
    }

    function format()
    {
        $s = "{\n";
        foreach ($this->members as $member) {
            $s .= "\t" . $member->format() . "\n";
        }
        $s .= "}\n";
    }
}

function parse_atom($lexer)
{
    $nono = ['case', 'default', 'if', 'else', 'for', 'while', 'switch'];
    if (in_array($lexer->peek()->type, $nono)) {
        throw new Exception("expression: unexpected keyword '" . $lexer->peek()->type . "'");
    }

    if ($lexer->peek()->type == '(' && is_type($lexer->peek(1), $lexer->typenames)) {
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

    if (is_prefix_op($lexer->peek())) {
        $op = $lexer->get()->type;
        return new c_prefix_operator($op, parse_expression($lexer));
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

        if (is_postfix_op($lexer->peek())) {
            $op = $lexer->get()->type;
            $result = new c_postfix_operator($result, $op);
            continue;
        }
        break;
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
        case 'switch':
            return c_switch::parse($lexer);
    }

    $expr = parse_expression($lexer);
    expect($lexer, ';', 'parsing statement');
    return $expr;
}
