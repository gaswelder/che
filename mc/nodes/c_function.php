<?php

class c_function
{
    private $type;
    private $form;
    private $parameters;
    private $body;

    static function parse($lexer)
    {
        $self = new self;
        $self->type = c_type::parse($lexer);
        $self->form = c_form::parse($lexer);
        $self->parameters = c_function_parameters::parse($lexer);
        $self->body = c_body::parse($lexer);
        return $self;
    }

    function format()
    {
        return sprintf(
            "%s %s%s %s",
            $this->type->format(),
            $this->form->format(),
            $this->parameters->format(),
            $this->body->format()
        );
    }
}
