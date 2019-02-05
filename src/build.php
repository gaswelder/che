<?php

function cmd_build($path, $name)
{
    $m = parse_path($path);
    $mods = resolve_deps($m);
    $c_mods = array_map('translate', $mods);
    build($c_mods, $name);
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
    // echo "$cmd\n";
    exec($cmd, $output, $ret);
}