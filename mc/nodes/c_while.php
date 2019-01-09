<?php

class c_while
{
    private $condition;
    private $body;

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, 'while');
        expect($lexer, '(');
        $self->condition = parse_expression($lexer);
        expect($lexer, ')');
        $self->body = c_body::parse($lexer);
        return $self;
    }

    function format()
    {
        return sprintf('while (%s) %s', format_expression($this->condition), $this->body->format());
    }
}
