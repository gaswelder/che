<?php

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
        $types = array_merge($types, package_file_typenames($path));
    }

    // Parse each file separately using the gathered types information.
    $modules = array_map(function ($path) use ($types) {
        $lexer = new lexer($path);
        $lexer->typenames = $types;
        try {
            return parse_module($lexer);
        } catch (Exception $e) {
            $next = $lexer->peek();
            $where = "$path:" . $lexer->peek()['pos'];
            $what = $e->getMessage();
            echo sprintf("%s: %s: %s...\n", $where, $what, token_to_string($next));
        }
    }, $paths);

    // Merge all partial modules into one.
    $result = array_reduce($modules, function ($a, $b) {
        if (!$a) return $b;
        return merge_modules($a, $b);
    }, null);

    return $result;
}

function resolve_deps(c_module $m)
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
function package_file_typenames($path): array
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
        $name = expect($lexer, 'word')['content'];
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
