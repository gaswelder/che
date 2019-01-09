<?php

class c_return
{
    private $expression;

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, 'return');
        $self->expression = parse_expression($lexer);
        expect($lexer, ';');
        return $self;
    }

    function format()
    {
        return 'return ' . format_expression($this->expression) . ';';
    }
}