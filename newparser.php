<?php
require 'src/lexer.php';
require 'src/token.php';
require 'mc/debug.php';
require 'src/package_file.php';
require 'src/parser.php';
require 'src/translator.php';
require 'src/resolver.php';
require 'src/build.php';
require 'src/deptree.php';
foreach (glob('src/nodes/*.php') as $path) {
    require $path;
}

function indent($text, $tab = "\t")
{
    if (substr($text, -1) == "\n") {
        return indent(substr($text, 0, -1), $tab) . "\n";
    }
    return $tab . str_replace("\n", "\n$tab", $text);
}

// $path = 'test/expressions.c';
// $m = parse_path($path);
// echo $m->format();
// exit;

// Build
// cmd_build('prog/xml.c', 'zz');
// exit;
foreach (glob('prog/*.c') as $path) {
    echo $path, "\n";
    cmd_build($path, 'bin/' . basename($path));
}
exit;

// Deps
// $path = 'prog/chargen.c';
// echo dep_tree($path);
// exit;
// foreach (glob('test/*.c') as $path) {
//     echo dep_tree($path);
// }

