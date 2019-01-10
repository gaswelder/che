<?php

class c_function_call
{
    private $function;
    private $arguments;

    function __construct($function, $arguments)
    {
        $this->function = $function;
        $this->arguments = $arguments;
    }

    function format()
    {
        return $this->function->format() . $this->arguments->format();
    }
}
