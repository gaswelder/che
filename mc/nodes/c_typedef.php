<?php

class c_typedef
{
    private $type;
    private $alias;

    static function parse($lexer)
    {
        expect($lexer, 'typedef');
        $self = new self;
        $self->type = c_type::parse($lexer);
        $self->alias = c_identifier::parse($lexer);
        expect($lexer, ';');
        return $self;
    }

    function format()
    {
        return sprintf("typedef %s %s;\n", $this->type->format(), $this->alias->format());
    }
}
