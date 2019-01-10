<?php

class c_cast
{
    private $type;
    private $operand;

    static function parse($lexer)
    {
        expect($lexer, '(');
        $self = new self;
        $self->type = c_type::parse($lexer);
        expect($lexer, ')');
        $self->operand = parse_expression($lexer);
        return $self;
    }

    function format()
    {
        return '(' . $this->type->format() . ') ' . $this->operand->format();
    }
}
