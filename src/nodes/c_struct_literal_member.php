<?php

class c_struct_literal_member
{
    public $name;
    public $value;

    function format()
    {
        return '.' . $this->name->format() . ' = ' . $this->value->format();
    }
}
