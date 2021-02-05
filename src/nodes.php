<?php

class c_compat_function_declaration
{
    public $static;
    public $type;
    public $form;
    public $parameters;
    public $body;

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
        return [
            'kind' => 'c_compat_function_forward_declaration',
            'static' => $this->static,
            'type' => $this->type,
            'form' => $this->form,
            'parameters' => $this->parameters
        ];
    }
}
