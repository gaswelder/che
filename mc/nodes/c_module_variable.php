<?php

class c_module_variable
{
    private $type;
    private $form;
    private $value;

    function __construct($type, $form, $value)
    {
        $this->type = $type;
        $this->form = $form;
        $this->value = $value;
    }

    function format()
    {
        return $this->type->format()
            . ' ' . $this->form->format()
            . ' = ' . $this->value->format();
    }
}
