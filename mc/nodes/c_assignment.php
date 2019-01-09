<?php

class c_assignment
{
    private $name;
    private $value;

    static function parse($lexer)
    {
        $self = new self;
        $self->name = c_identifier::parse($lexer);
        expect($lexer, '=');
        $self->value = parse_expression($lexer);
        return $self;
    }

    function format()
    {
        return $this->name->format() . ' = ' . format_expression($this->value);
    }
}
