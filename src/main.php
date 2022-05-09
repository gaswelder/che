<?php
require 'src/formatter.php';
require 'src/translator.php';
require_once 'src/rust-proxy.php';

set_error_handler(function ($errno, $errstr, $file, $line) {
    throw new Exception("$errstr at $file:$line");
});

function indent($text, $tab = "\t")
{
    if (substr($text, -1) == "\n") {
        return indent(substr($text, 0, -1), $tab) . "\n";
    }
    return $tab . str_replace("\n", "\n$tab", $text);
}

exit(main($argv));

function main($argv)
{
    $commands = [
        'build' => 'cmd_build',
        'deptree' => 'cmd_deptree'
    ];
    array_shift($argv);
    if (empty($argv)) {
        echo "Usage: che <command>\n";
        foreach ($commands as $name => $func) {
            echo "\t$name\n";
        }
        return 1;
    }
    $cmd = array_shift($argv);
    if ($cmd == 'build') {
        return cmd_build($argv);
    }
    if ($cmd == 'deptree') {
        return cmd_deptree($argv);
    }

    echo "Unknown command: $cmd\n";
    return 1;
}

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

    if (!file_exists('tmp')) {
        mkdir('tmp');
    }

    // Save all the modules somewhere.
    $paths = [];
    foreach ($c_mods as $module) {
        $src = format_module($module);
        $path = 'tmp/' . md5($src) . '.c';
        file_put_contents($path, $src);
        $paths[] = $path;
    }

    $link = ['m'];
    foreach ($c_mods as $module) {
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

function resolve_deps($m)
{
    $deps = [$m];
    foreach (module_imports($m) as $imp) {
        $sub = get_module($imp['path']);
        $deps = array_merge($deps, resolve_deps($sub));
    }
    return array_unique($deps, SORT_REGULAR);
}

function module_imports($module)
{
    $list = [];
    foreach ($module['elements'] as $element) {
        if (is_array($element) && $element['kind'] === 'c_import') {
            $list[] = $element;
        }
    }
    return $list;
}

function cmd_deptree($argv)
{
    if (count($argv) != 1) {
        echo "Usage: deptree <source-path>\n";
        return 1;
    }
    $path = array_shift($argv);
    $m = get_module($path);
    $t = build_tree($m);
    $r = render_tree($path, $t);
    echo implode("\n", $r) . "\n";
}

function build_tree($module)
{
    $tree = [];
    $imports = module_imports($module);
    foreach ($imports as $import) {
        $tree[] = [$import->path, build_tree(get_module($import['path']))];
    }
    return $tree;
}

function render_tree($root, $tree)
{
    $s = [$root];
    $n = count($tree);
    foreach ($tree as $i => $dep) {
        [$subroot, $subtree] = $dep;
        $r = render_tree($subroot, $subtree);
        if ($i == $n - 1) {
            $s = array_merge($s, indent_tree($r, ' └', '  '));
        } else {
            $s = array_merge($s, indent_tree($r, ' ├', ' │'));
        }
    }
    return $s;
}

function indent_tree($lines, $first, $cont)
{
    $result = [];
    foreach ($lines as $i => $line) {
        if ($i == 0) {
            $result[] = $first . $line;
        } else {
            $result[] = $cont . $line;
        }
    }
    return $result;
}
