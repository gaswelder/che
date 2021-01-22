<?php

class c_defer
{
    public $expression;

    function format()
    {
        return 'defer ' . $this->expression->format() . ';';
    }
}
