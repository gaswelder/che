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
    return call_rust('parse_statement', $lexer, $typenames);
}

function parse_expression($lexer, $typenames, $current_strength)
{
    return call_rust('parse_expression', $lexer, $typenames, $current_strength);
}

function expect($lexer, $type, $comment = null)
{
    return call_rust('expect', $lexer, $type, $comment);
}

function is_type($name, $typenames)
{
    return call_rust_mem('is_type', $name, $typenames);
}

function operator_strength($op)
{
    return call_rust_mem("operator_strength", $op);
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
    $variadic = false;
    expect($lexer, '(');
    if (!$lexer->follows(')')) {
        $parameters[] = parse_function_parameter($lexer, $typenames);
        while ($lexer->follows(',')) {
            $lexer->get();
            if ($lexer->follows('...')) {
                $lexer->get();
                $variadic = true;
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
            'list' => $parameters,
            'variadic' => $variadic
        ],
        'body' => $body
    ];
}

function parse_form($lexer, $typenames)
{
    return call_rust('parse_form', $lexer, $typenames);
}

function parse_literal($lexer)
{
    return call_rust("parse_literal", $lexer);
}

function parse_identifier($lexer)
{
    return call_rust('parse_identifier', $lexer);
}
