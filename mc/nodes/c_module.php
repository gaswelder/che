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
}
