<?php

class c_module
{
    private $elements = [];

    static function parse($lexer)
    {
        $m = new self;
        while ($lexer->more()) {
            $m->elements[] = parse_module_element($lexer);
        }
        return $m;
    }

    function format()
    {
        $s = '';
        foreach ($this->elements as $node) {
            $s .= $node->format();
        }
        return $s;
    }

    function json()
    {
        return ['type' => 'c_module', 'elements' => array_map(function ($element) {
            return $element->json();
        }, $this->elements)];
    }

    function imports()
    {
        $list = [];
        foreach ($this->elements as $element) {
            if ($element instanceof c_import) {
                $list[] = $element;
            }
        }
        return $list;
    }

    function types()
    {
        $list = [];
        foreach ($this->elements as $element) {
            if ($element instanceof c_typedef) {
                $list[] = $element->name();
            }
        }
        return $list;
    }

    function merge(c_module $that)
    {
        $result = new self;
        $result->elements = array_merge($this->elements, $that->elements);
        return $result;

		// // Remove redundant imports
        // $imports = [];
        // $mod->code = array_filter($mod->code, function ($c) use (&$imports) {
        //     if ($c instanceof che_import) {
        //         if (in_array($c->path(), $imports)) {
        //             return false;
        //         }
        //         $imports[] = $c->path();
        //     }
        //     return true;
        // });

        // return $mod;
    }
}
