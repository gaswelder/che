<?php

class c_compat_function_declaration
{
    private $static;
    private $type;
    private $form;
    private $parameters;
    private $body;

    function __construct($static, $type, $form, $parameters, $body)
    {
        $this->static = $static;
        $this->type = $type;
        $this->form = $form;
        $this->parameters = $parameters;
        $this->body = $body;
    }

    function forward_declaration()
    {
        return new c_compat_function_forward_declaration($this->static, $this->type, $this->form, $this->parameters);
    }

    function is_static()
    {
        return $this->static;
    }

    function format()
    {
        $s = '';
        if ($this->static && $this->form->format() != 'main') {
            $s .= 'static ';
        }
        $s .= $this->type->format()
            . ' ' . $this->form->format()
            . $this->parameters->format()
            . ' ' . $this->body->format();
        return $s;
    }
}
