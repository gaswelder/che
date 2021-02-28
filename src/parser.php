<?php

function get_module($name)
{
    if (substr($name, -2) === ".c") {
        $module_path = $name;
    } else {
        $module_path = "lib/$name.c";
    }
    if (!file_exists($module_path)) {
        throw new Exception("can't find module '$name'");
    }

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

function get_file_typenames($path): array
{
    return call_rust('get_file_typenames', $path);
}

function get_typename(lexer $lexer)
{
    return call_rust('get_typename', $lexer);
}

function parse_module($lexer, $typenames)
{
    $elements = [];
    while ($lexer->more()) {
        switch ($lexer->peek()['kind']) {
            case 'import':
                $import = parse_import($lexer);
                $elements[] = $import;

                $module = get_module($import['path']);
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
