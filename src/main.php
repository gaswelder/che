<?php
require 'src/formatter.php';
require 'src/modules.php';
require 'src/translator.php';
require 'src/cmd/build.php';
require 'src/cmd/deptree.php';
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
    if (!isset($commands[$cmd])) {
        echo "Unknown command: $cmd\n";
        return 1;
    }

    $func = $commands[$cmd];
    return $func($argv);
}
