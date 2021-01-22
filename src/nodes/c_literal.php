<?php

class c_literal
{
    public $value;
    public $type;

    function format()
    {
        switch ($this->type) {
            case 'string':
                return '"' . $this->value . '"';
            case 'char':
                return '\'' . $this->value . '\'';
            default:
                return $this->value;
        }
    }
}
