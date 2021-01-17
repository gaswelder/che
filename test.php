<?php

function test($name, $f)
{
    try {
        $f();
        echo "OK: $name\n";
    } catch (Exception $e) {
        echo "FAIL: $name: " . $e->getMessage(), "\n";
    }
}

function eq($expected, $actual)
{
    if ($expected !== $actual) {
        throw new Exception("expected '$expected', got '$actual'");
    }
}

function lsr($dir)
{
    $h = opendir($dir);
    for (;;) {
        $name = readdir($h);
        if ($name === false) {
            break;
        }
        if ($name === '.' || $name === '..') {
            continue;
        }
        $path = "$dir/$name";
        if (is_dir($path)) {
            yield from lsr($path);
        } else {
            yield $path;
        }
    }
}

foreach (lsr('src') as $n) {
    if (preg_match('/\.test\.php$/', $n)) {
        echo $n, "\n";
        require $n;
    }
}
