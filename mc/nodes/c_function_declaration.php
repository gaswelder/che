<?php

class c_function_declaration
{
    private $type;
    private $form;
    private $parameters;
    private $body;

    function __construct($type, $form, $parameters, $body)
    {
        $this->type = $type;
        $this->form = $form;
        $this->parameters = $parameters;
        $this->body = $body;
    }

    function format()
    {
        return sprintf(
            "%s %s%s %s\n\n",
            $this->type->format(),
            $this->form->format(),
            $this->parameters->format(),
            $this->body->format()
        );
    }
}
