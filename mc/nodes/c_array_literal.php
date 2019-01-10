<?php

class c_array_literal
{
    private $values = [];

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, '{', 'array literal');
        while ($lexer->more() && $lexer->peek()->type != '}') {
            $self->values[] = expect($lexer, 'num', 'array literal')->content;
        }
        expect($lexer, '}', 'array literal');
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
