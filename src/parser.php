<?php

function parse_path($module_path)
{
    $types = get_file_typenames($module_path);
    $lexer = new lexer($module_path);
    try {
        return parse_module($lexer, $types);
    } catch (Exception $e) {
        $next = $lexer->peek();
        $where = "$module_path:" . $lexer->peek()['pos'];
        $what = $e->getMessage();
        echo sprintf("%s: %s: %s...\n", $where, $what, token_to_string($next));
    }
}

function parse_module($lexer, $typenames)
{
    $elements = [];
    while ($lexer->more()) {
        switch ($lexer->peek()['kind']) {
            case 'import':
                $import = parse_import($lexer);
                $elements[] = $import;

                $module = resolve_import($import);
                foreach ($module['elements'] as $element) {
                    if ($element['kind'] == 'c_typedef') {
                        $typenames[] = format_node($element['alias']);
                    }
                }
                break;
            case 'typedef':
                $elements[] = parse_typedef($lexer, $typenames);
                break;
            case 'macro':
                $elements[] = parse_compat_macro($lexer);
                break;
            default:
                $elements[] = parse_module_object($lexer, $typenames);
        }
    }
    return [
        'kind' => 'c_module',
        'elements' => $elements
    ];
}

function parse_module_object($lexer, $typenames)
{
    return call_rust('parse_module_object', $lexer, $typenames);
}

function parse_anonymous_parameters($lexer)
{
    return call_rust('parse_anonymous_parameters', $lexer);
}

function parse_import($lexer)
{
    return call_rust('parse_import', $lexer);
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

function expect($lexer, $type, $comment = null)
{
    return call_rust('expect', $lexer, $type, $comment);
}

function operator_strength($op)
{
    return call_rust_mem("operator_strength", $op);
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
    return call_rust('parse_union', $lexer, $typenames);
}

function parse_type($lexer, $comment = null)
{
    return call_rust('parse_type', $lexer, $comment);
}

function parse_form($lexer, $typenames)
{
    return call_rust('parse_form', $lexer, $typenames);
}

function parse_identifier($lexer)
{
    return call_rust('parse_identifier', $lexer);
}
