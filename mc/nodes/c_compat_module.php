<?php

class c_compat_module
{
    private $elements = [];
    private $sources = [];
    private $link = [];

    private $path;

    function __construct($elements, $sources)
    {
        $this->elements = $elements;
        $this->sources = $sources;
    }

    function format()
    {
        $s = '';
        foreach ($this->elements as $node) {
            $s .= $node->format();
        }
        return $s;
    }

    function synopsis()
    {
        $elements = [];

        foreach ($this->elements as $element) {
            if ($element instanceof c_compat_function_declaration && !$element->is_static()) {
                $elements[] = $element->forward_declaration();
                continue;
            }
            if ($element instanceof c_typedef) {
                $elements[] = $element;
                continue;
            }
            if ($element instanceof c_struct_definition) {
                $elements[] = $element;
                continue;
            }
        }
        return $elements;
    }

    function source_path()
    {
        if (!$this->path) {
            if (!file_exists('tmp')) {
                mkdir('tmp');
            }
            $s = $this->format();
            $this->path = 'tmp/' . md5($s) . '.c';
            file_put_contents($this->path, $s);
        }
        return $this->path;
    }

    function build($name)
    {
        if (!file_exists('tmp')) {
            mkdir('tmp');
        }
        $s = $this->format();
        file_put_contents("tmp/$name.c", $s);

        $sources = array_merge(["tmp/$name.c"], $this->sources);

        $cmd = 'c99 -Wall -Wextra -Werror -pedantic -pedantic-errors';
        $cmd .= ' -fmax-errors=3';
        $cmd .= ' -g ' . implode(' ', $sources);
        $cmd .= ' -o ' . $name;
        foreach ($this->link as $name) {
            $cmd .= ' -l ' . $name;
        }
        echo "$cmd\n";
        exec($cmd, $output, $ret);
    }
}
