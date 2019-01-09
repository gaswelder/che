<?php

class c_defer
{
    private $expression;

    static function parse($lexer)
    {
        expect($lexer, 'defer');
        $self = new self;
        $self->expression = parse_expression($lexer);
        expect($lexer, ';');
        return $self;
    }

    function format()
    {
        return 'defer ' . format_expression($this->expression) . ';';
    }
}
