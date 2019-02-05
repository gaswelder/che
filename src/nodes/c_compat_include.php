<?php

class c_compat_include
{
    private $name;

    function __construct($name)
    {
        $this->name = $name;
    }

    function format()
    {
        return "#include $this->name\n";
    }
}
