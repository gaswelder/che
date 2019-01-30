<?php

function dep_tree($path)
{
    $m = parse_path($path);
    $t = build_tree($m);
    $r = render_tree($path, $t);
    return implode("\n", $r) . "\n";
}

function build_tree(c_module $module)
{
    $tree = [];
    $imps = $module->imports();
    foreach ($imps as $imp) {
        $tree[] = [$imp->name(), build_tree(resolve_import($imp))];
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
