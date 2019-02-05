<?php

class c_compat_struct_definition
{
    private $name;
    private $fields;

    function __construct(string $name, c_composite_type $fields)
    {
        $this->name = $name;
        $this->fields = $fields;
    }

    function format()
    {
        return 'struct ' . $this->name . ' ' . $this->fields->format() . ";\n";
    }
}
