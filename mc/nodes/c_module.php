<?php

class c_module
{
    private $elements = [];

    static function parse($lexer)
    {
        $m = new self;

        // expect a bunch of imports
        while ($lexer->more() && $lexer->peek()->type == 'import') {
            $m->elements[] = c_import::parse($lexer);
        }

        // a bunch of typedefs
        while ($lexer->more() && $lexer->peek()->type == 'typedef') {
            $m->elements[] = c_typedef::parse($lexer);
        }

        // expect a bunch of functions
        while ($lexer->more()) {
            $m->elements[] = c_function::parse($lexer);
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
