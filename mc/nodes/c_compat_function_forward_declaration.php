<?php

class c_compat_function_forward_declaration
{
    private $static;
    private $type;
    private $form;
    private $parameters;

    function __construct($static, $type, $form, $parameters)
    {
        $this->static = $static;
        $this->type = $type;
        $this->form = $form;
        $this->parameters = $parameters;
    }

    function format()
    {
        $s = '';
        if ($this->static && $this->form->format() != 'main') {
            $s .= 'static ';
        }
        $s .= $this->type->format()
            . ' ' . $this->form->format()
            . $this->parameters->format() . ";\n";
        return $s;
    }
}
