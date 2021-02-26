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
                        $typenames[] = format_node($element['form']['alias']);
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
    return call_rust('parse_typedef', $lexer, $typenames);
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
    return call_rust('parse_compat_macro', $lexer);
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
