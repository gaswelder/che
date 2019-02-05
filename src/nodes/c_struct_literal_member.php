<?php

class c_struct_literal_member
{
    private $name;
    private $value;

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, '.', 'struct literal member');
        $self->name = c_identifier::parse($lexer, 'struct literal member');
        expect($lexer, '=', 'struct literal member');
        $self->value = parse_expression($lexer, 'struct literal member');
        return $self;
    }

    function format()
    {
        return '.' . $this->name->format() . ' = ' . $this->value->format();
    }
}
