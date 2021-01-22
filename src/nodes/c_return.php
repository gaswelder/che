<?php

class c_return
{
    public $expression;

    function format()
    {
        if (!$this->expression) {
            return 'return;';
        }
        return 'return ' . $this->expression->format() . ';';
    }
}
