<?php

function cmd_build($argv)
{
    if (count($argv) < 1 || count($argv) > 2) {
        echo "Usage: build <source-path> [<output-path>]\n";
        return 1;
    }
    $path = array_shift($argv);
    $name = array_shift($argv);
    if (!$name) {
        $name = str_replace('.c', '', basename($path));
    }

    $m = get_module($path);
    if (!$m) {
        return 1;
    }
    $mods = resolve_deps($m);
    $c_mods = array_map('translate', $mods);
    return build($c_mods, $name);
}

function resolve_deps($m)
{
    $deps = [$m];
    foreach (module_imports($m) as $imp) {
        $sub = get_module($imp['path']);
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
        $src = format_module($module);
        $path = 'tmp/' . md5($src) . '.c';
        file_put_contents($path, $src);
        $paths[] = $path;
    }

    $link = ['m'];
    foreach ($modules as $module) {
        $link = array_unique(array_merge($link, $module['link']));
    }

    $cmd = 'c99 -Wall -Wextra -Werror -pedantic -pedantic-errors';
    $cmd .= ' -fmax-errors=3';
    $cmd .= ' -g ' . implode(' ', $paths);
    $cmd .= ' -o ' . $name;
    foreach ($link as $name) {
        $cmd .= ' -l ' . $name;
    }
    // echo "$cmd\n";
    exec($cmd, $output, $ret);
    return $ret;
}
