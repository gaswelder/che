<?php

class c_array_literal
{
    private $values = [];

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, '{');
        while ($lexer->more() && $lexer->peek()->type != '}') {
            $self->values[] = expect($lexer, 'num')->content;
        }
        expect($lexer, '}');
        return $self;
    }

    function format()
    {
        $s = '{';
        foreach ($this->values as $i => $value) {
            if ($i > 0) $s .= ', ';
            $s .= $value;
        }
        $s .= '}';
        return $s;
    }
}
