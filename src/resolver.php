<?php

function resolve_import($import)
{
    $name = $import['path'];
    $module_path = "lib/$name.c";
    if (!file_exists($module_path)) {
        throw new Exception("can't find module '$name'");
    }
    return parse_path($module_path);
}

function resolve_deps($m)
{
    $deps = [$m];
    foreach (module_imports($m) as $imp) {
        $sub = resolve_import($imp);
        $deps = array_merge($deps, resolve_deps($sub));
    }
    return array_unique($deps, SORT_REGULAR);
}

/**
 * Returns list of type names (as strings) declared in this file.
 */
function get_file_typenames($path): array
{
    $list = array();
    // Scan file tokens for 'typedef' keywords
    $lexer = new lexer($path);
    while (1) {
        $t = $lexer->get();
        if ($t === null) {
            break;
        }

        // When a 'typedef' is encountered, look ahead
        // to find the type name
        if ($t['kind'] == 'typedef') {
            $list[] = get_typename($lexer);
        }

        if ($t['kind'] == 'macro' && strpos($t['content'], '#type') === 0) {
            $list[] = trim(substr($t['content'], strlen('#type') + 1));
        }
    }
    return $list;
}

function get_typename(lexer $lexer)
{
    // The type name is at the end of the typedef statement.
    // typedef foo bar;
    // typedef {...} bar;
    // typedef struct foo bar;

    $skip_brackets = function () use ($lexer, &$skip_brackets) {
        expect($lexer, '{');
        while ($lexer->more()) {
            if ($lexer->follows('{')) {
                $skip_brackets();
                continue;
            }
            if ($lexer->follows('}')) {
                break;
            }
            $lexer->get();
        }
        expect($lexer, '}');
    };

    if ($lexer->follows('{')) {
        $skip_brackets();
        $x = expect($lexer, 'word');
        $name = $x['content'];
        expect($lexer, ';');
        return $name;
    }


    // Get all tokens until the semicolon.
    $buf = array();
    while (!$lexer->ended()) {
        $t = $lexer->get();
        if ($t['kind'] == ';') {
            break;
        }
        $buf[] = $t;
    }

    if (empty($buf)) {
        throw new Exception("No tokens after 'typedef'");
    }

    $buf = array_reverse($buf);

    // We assume that function typedefs end with "(...)".
    // In that case we omit that part.
    if ($buf[0]['kind'] == ')') {
        while (!empty($buf)) {
            $t = array_shift($buf);
            if ($t['kind'] == '(') {
                break;
            }
        }
    }

    // The last 'word' token is assumed to be the type name.
    $name = null;
    while (!empty($buf)) {
        $t = array_shift($buf);
        if ($t['kind'] == 'word') {
            $name = $t['content'];
            break;
        }
    }

    if (!$name) {
        throw new Exception("Type name expected in the typedef");
    }
    return $name;
}
