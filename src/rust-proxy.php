<?php

function call_rust($f, ...$args)
{
    $descriptorspec = array(
        0 => array("pipe", "r"),  // stdin is a pipe that the child will read from
        1 => array("pipe", "w"),  // stdout is a pipe that the child will write to
        // 2 => array("file", "/tmp/error-output.txt", "a") // stderr is a file to write to
    );

    $proc = proc_open("cargo run -q", $descriptorspec, $pipes);
    fwrite($pipes[0], json_encode(["f" => $f, "a" => $args]) . "\n");
    fclose($pipes[0]);
    $s = stream_get_contents($pipes[1]);
    proc_terminate($proc);
    $data = json_decode($s, true);
    if ($data["error"] !== "") {
        throw new Exception($data["error"]);
    }
    return $data["data"];
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
    return new class ($name, $args)
    {
        private $proc;
        private $pipes;

        function __construct($name, $args)
        {
            $descriptorspec = array(
                0 => array("pipe", "r"),  // stdin is a pipe that the child will read from
                1 => array("pipe", "w"),  // stdout is a pipe that the child will write to
                // 2 => array("file", "/tmp/error-output.txt", "a") // stderr is a file to write to
            );

            $proc = proc_open("cargo run -q $name", $descriptorspec, $pipes);
            fwrite($pipes[0], json_encode(["f" => '__construct', "a" => $args]) . "\n");
            $s = json_decode(fgets($pipes[1]), true);
            if ($s['error']) {
                throw new Exception($s['error']);
            }
            $this->proc = $proc;
            $this->pipes = $pipes;
        }

        function __destruct()
        {
            // fclose($this->pipes[0]);
            proc_terminate($this->proc);
        }

        function __call($method, $arguments)
        {
            $pipes = $this->pipes;
            fwrite($pipes[0], json_encode(["f" => $method, "a" => $arguments]) . "\n");
            $s = json_decode(fgets($pipes[1]), true);
            if ($s['error']) {
                throw new Exception($s['error']);
            }
            return $s['data'];
        }
    };
}
