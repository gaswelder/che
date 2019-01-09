<?php

class c_function_parameter
{
    private $type;
    private $form;

    static function parse($lexer)
    {
        $self = new self;
        $self->type = c_type::parse($lexer);
        $self->form = c_form::parse($lexer);
        return $self;
    }

    function format()
    {
        return $this->type->format() . ' ' . $this->form->format();
    }
}
