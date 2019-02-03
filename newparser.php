<?php

require 'mc/parser/lexer.php';
require 'mc/parser/token.php';
require 'mc/debug.php';
require 'mc/module/package_file.php';
require 'deptree.php';
foreach (glob('mc/nodes/*.php') as $path) {
    require $path;
}

// $path = 'test/expressions.c';
// $m = parse_path($path);
// echo $m->format();
// exit;

// Build
cmd_build('prog/lexer.c', 'zz');
exit;
foreach (glob('prog/*.c') as $path) {
    echo $path, "\n";
    cmd_build($path, 'z-' . basename($path));
}
exit;

function cmd_build($path, $name)
{
    $m = parse_path($path);
    $mods = resolve_deps($m);
    $c_mods = array_map('translate', $mods);
    build($c_mods, $name);
}

// Deps
// $path = 'prog/chargen.c';
// echo dep_tree($path);
// exit;

function translate(c_module $m)
{
    [$elements, $link] = translate_module($m->elements());
    return new c_compat_module($elements, $link);
}

function resolve_deps(c_module $m)
{
    $deps = [$m];
    foreach ($m->imports() as $imp) {
        $sub = resolve_import($imp);
        $deps = array_merge($deps, resolve_deps($sub));
    }
    return array_unique($deps, SORT_REGULAR);
}

function build($modules, $name)
{
    if (!file_exists('tmp')) {
        mkdir('tmp');
    }

    // Save all the modules somewhere.
    $paths = [];
    foreach ($modules as $module) {
        $src = $module->format();
        $path = 'tmp/' . md5($src) . '.c';
        file_put_contents($path, $src);
        $paths[] = $path;
    }

    $link = [];
    foreach ($modules as $module) {
        $link = array_unique(array_merge($link, $module->link()));
    }

    $cmd = 'c99 -Wall -Wextra -Werror -pedantic -pedantic-errors';
    $cmd .= ' -fmax-errors=3';
    $cmd .= ' -g ' . implode(' ', $paths);
    $cmd .= ' -o ' . $name;
    foreach ($link as $name) {
        $cmd .= ' -l ' . $name;
    }
    echo "$cmd\n";
    exec($cmd, $output, $ret);
}


foreach (glob('test/*.c') as $path) {
    echo dep_tree($path);
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

function hoist_declarations($elements)
{
    $order = [
        [c_compat_include::class, c_compat_macro::class],
        [c_struct_definition::class, c_typedef::class, c_compat_enum::class],
        [c_compat_function_forward_declaration::class]
    ];

    $get_order = function ($element) use ($order) {
        foreach ($order as $i => $classnames) {
            if (in_array(get_class($element), $classnames)) {
                return $i;
            }
        }
        return count($order);
    };

    $groups = [[], [], [], []];

    foreach ($elements as $element) {
        $groups[$get_order($element)][] = $element;
    }
    return call_user_func_array('array_merge', $groups);
}

function translate_module($che_elements)
{
    $elements = [];
    $link = [];

    foreach ($che_elements as $element) {
        if ($element instanceof c_import) {
            $module = resolve_import($element);
            $compat = translate($module);
            $elements = array_merge($elements, $compat->synopsis());
            continue;
        }
        if ($element instanceof c_function_declaration) {
            $func = $element->translate();
            $elements[] = $func->forward_declaration();
            $elements[] = $func;
            continue;
        }
        if ($element instanceof c_enum) {
            $elements[] = $element->translate();
            continue;
        }
        // Discard #type hints.
        if ($element instanceof c_compat_macro && $element->name() == 'type') {
            continue;
        }
        // Discard #link hints, but remember the values.
        if ($element instanceof c_compat_macro && $element->name() == 'link') {
            $link[] = $element->value();
            continue;
        }
        $elements[] = $element;
    }

    $std = [
        'assert',
        'ctype',
        'errno',
        'limits',
        'math',
        'stdarg',
        'stdbool',
        'stddef',
        'stdint',
        'stdio',
        'stdlib',
        'string',
        'time'
    ];
    foreach ($std as $n) {
        $elements[] = new c_compat_include("<$n.h>");
    }

    return [deduplicate_synopsis(hoist_declarations($elements)), $link];
}

function deduplicate_synopsis($elements)
{
    $result = [];
    $set = [];

    foreach ($elements as $element) {
        if ($element instanceof c_typedef || $element instanceof c_struct_definition) {
            $s = $element->format();
            if (isset($set[$s])) {
                continue;
            }
            $set[$s] = true;
        }
        $result[] = $element;
    }

    return $result;
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
        ['prefix'],
        ['->', '.']
    ];
    foreach ($map as $i => $ops) {
        if (in_array($op, $ops)) {
            return $i + 1;
        }
    }
    throw new Exception("unknown operator: '$op'");
}

function parse_expression($lexer, $current_strength = 0)
{
    $result = parse_atom($lexer);
    while (is_op($lexer->peek())) {
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
            $s .= "\t" . $member->format() . ",\n";
        }
        $s .= "}\n";
        return $s;
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

        if (is_postfix_op($lexer->peek())) {
            $op = $lexer->get()->type;
            $result = new c_postfix_operator($result, $op);
            continue;
        }
        break;
    }

    return $result;
}

function indent($text, $tab = "\t")
{
    if (substr($text, -1) == "\n") {
        return indent(substr($text, 0, -1), $tab) . "\n";
    }
    return $tab . str_replace("\n", "\n$tab", $text);
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
