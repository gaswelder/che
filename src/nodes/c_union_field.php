<?php

class c_union_field
{
    private $type;
    private $form;

    static function parse($lexer)
    {
        $self = new self;
        $self->type = c_type::parse($lexer);
        $self->form = c_form::parse($lexer);
        expect($lexer, ';');
        return $self;
    }

    function format()
    {
        return $this->type->format() . ' ' . $this->form->format() . ';';
    }
}
