<?php

class c_loop_counter_declaration
{
    private $type;
    private $name;
    private $value;

    static function parse($lexer)
    {
        $self = new self;
        $self->type = c_type::parse($lexer);
        $self->name = c_identifier::parse($lexer);
        expect($lexer, '=');
        $self->value = parse_expression($lexer);
        return $self;
    }

    function format()
    {
        $s = sprintf(
            '%s %s = %s',
            $this->type->format(),
            $this->name->format(),
            format_expression($this->value)
        );
        return $s;
    }
}
