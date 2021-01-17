<?php

$fails = 0;

function test($name, $f)
{
    global $fails;
    try {
        $f();
        echo "OK: $name\n";
    } catch (Exception $e) {
        echo sprintf("FAIL: $name: %s\n", $e->getMessage());
        $fails++;
    }
}

function formatValue($v)
{
    if ($v === null) {
        return 'NULL';
    }
    return "'$v'";
}

function eq($expected, $actual)
{
    if (is_array($expected) && is_array($actual)) {
        $keys = array_unique(array_merge(array_keys($expected), array_keys($actual)));
        $errors = [];
        foreach ($keys as $key) {
            if (array_key_exists($key, $expected) && !array_key_exists($key, $actual)) {
                $errors[] = "missing: $key = " . $expected[$key];
                continue;
            }
            if (!array_key_exists($key, $expected) && array_key_exists($key, $actual)) {
                $errors[] = "unexpected: $key = " . $actual[$key];
                continue;
            }
            if ($actual[$key] !== $expected[$key]) {
                $errors[] = sprintf("mismatch:\n\tactual  [$key] = %s\n\texpected[$key] = %s", formatValue($actual[$key]), formatValue($expected[$key]));
            }
        }
        if (count($errors) > 0) {
            throw new Exception(implode("\n", $errors));
        }
        return;
    }
    if ($expected !== $actual) {
        throw new Exception(sprintf("expected %s, got %s", formatValue($expected), formatValue($actual)));
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

exit($fails);
