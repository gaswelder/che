<?php

class c_compat_struct_forward_declaration
{
    private $name;

    function __construct($name)
    {
        $this->name = $name;
    }

    function format()
    {
        return 'struct ' . $this->name->format() . ";\n";
    }
}
