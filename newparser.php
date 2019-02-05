<?php
require 'src/lexer.php';
require 'src/token.php';
require 'mc/debug.php';
require 'src/package_file.php';
require 'src/parser.php';
require 'src/translator.php';
require 'src/resolver.php';
require 'src/cmd/build.php';
require 'src/cmd/deptree.php';
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
    if (!isset($commands[$cmd])) {
        echo "Unknown command: $cmd\n";
        return 1;
    }

    $func = $commands[$cmd];
    $func($argv);
    return 0;
}
