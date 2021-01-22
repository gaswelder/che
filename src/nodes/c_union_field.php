<?php

class c_union_field
{
    public $type;
    public $form;

    function format()
    {
        return $this->type->format() . ' ' . $this->form->format() . ';';
    }
}
