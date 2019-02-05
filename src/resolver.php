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

function resolve_deps(c_module $m)
{
    $deps = [$m];
    foreach ($m->imports() as $imp) {
        $sub = resolve_import($imp);
        $deps = array_merge($deps, resolve_deps($sub));
    }
    return array_unique($deps, SORT_REGULAR);
}