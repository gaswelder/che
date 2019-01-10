<?php

class c_if
{
    private $condition;
    private $body;
    private $else = null;

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, 'if');
        expect($lexer, '(');
        $self->condition = parse_expression($lexer);
        expect($lexer, ')');
        $self->body = c_body::parse($lexer);
        if ($lexer->peek()->type == 'else') {
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
