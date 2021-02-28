<?php

function make_call($ns, $id, $f, $args)
{
    $conn = fsockopen("localhost", 2124, $errno, $errstr);
    if ($errno) {
        throw new Exception("make_call: $errstr ($errno)");
    }

    $fargs = array_map(function ($arg) {
        // if ($arg instanceof buf) {
        //     return $arg->_rust_instance_id;
        // }
        return $arg;
    }, $args);

    fwrite($conn, json_encode(['ns' => $ns, 'id' => $id, "f" => $f, "a" => $fargs]) . "\n");
    $line = fgets($conn);
    fclose($conn);
    $s = json_decode($line, true);
    if ($s['error']) {
        throw new Exception($s['error']);
    }
    return $s['data'];
}

function call_rust($f, ...$args)
{
    $fargs = array_map(function ($arg) {
        if ($arg instanceof lexer) {
            return $arg->proxy->_rust_instance_id;
        }
        return $arg;
    }, $args);
    return make_call('', '', $f, $fargs);
}

$_call_rust_mem = [];
function call_rust_mem($f, ...$args)
{
    global $_call_rust_mem;
    $s = json_encode(["f" => $f, "a" => $args]);
    if (!array_key_exists($s, $_call_rust_mem)) {
        $_call_rust_mem[$s] = call_rust($f, ...$args);
    }
    return $_call_rust_mem[$s];
}

function make_rust_object($name, ...$args)
{
    return new class($name, $args)
    {
        public $_rust_instance_id;
        private $ns;

        function __construct($name, $args)
        {
            $s = make_call($name, '', '__construct', $args);
            $this->_rust_instance_id = $s;
            $this->ns = $name;
        }

        function __call($method, $args)
        {
            return make_call($this->ns, $this->_rust_instance_id, $method, $args);
        }
    };
}

function get_module($name)
{
    return call_rust('get_module', $name);
}

function operator_strength($op)
{
    return call_rust_mem("operator_strength", $op);
}

class lexer
{
    public $proxy;

    function __construct($path)
    {
        $this->proxy = make_rust_object('lexer', $path);
    }

    function __call($f, $args)
    {
        return call_user_func_array([$this->proxy, $f], $args);
    }
}
