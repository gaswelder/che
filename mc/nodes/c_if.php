<?php

class c_if
{
    private $condition;
    private $body;
    private $else = null;

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, 'if', 'if statement');
        expect($lexer, '(', 'if statement');
        $self->condition = parse_expression($lexer);
        expect($lexer, ')', 'if statement');
        $self->body = c_body::parse($lexer);
        if ($lexer->follows('else')) {
            $lexer->get();
            $self->else = c_body::parse($lexer);
        }
        return $self;
    }

    function format()
    {
        $s = sprintf('if (%s) %s', $this->condition->format(), $this->body->format());
        if ($this->else) {
            $s .= ' else ' . $this->else->format();
        }
        return $s;
    }
}
