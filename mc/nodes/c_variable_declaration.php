<?php

class c_variable_declaration
{
    private $type;
    private $form;
    private $value;

    static function parse($lexer)
    {
        $self = new self;
        $self->type = c_type::parse($lexer);
        $self->form = c_form::parse($lexer);
        expect($lexer, '=');
        $self->value = parse_expression($lexer);
        expect($lexer, ';');
        return $self;
    }

    function format()
    {
        return $this->type->format() . ' '
            . $this->form->format() . ' = '
            . format_expression($this->value);
    }
}
