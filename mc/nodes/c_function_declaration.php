<?php

class c_function_declaration
{
    private $pub;
    private $type;
    private $form;
    private $parameters;
    private $body;

    function __construct($pub, $type, $form, $parameters, $body)
    {
        $this->pub = $pub;
        $this->type = $type;
        $this->form = $form;
        $this->parameters = $parameters;
        $this->body = $body;
    }

    function format()
    {
        $s = sprintf(
            "%s %s%s %s\n\n",
            $this->type->format(),
            $this->form->format(),
            $this->parameters->format(),
            $this->body->format()
        );
        if ($this->pub) {
            return 'pub ' . $s;
        }
        return $s;
    }
}
