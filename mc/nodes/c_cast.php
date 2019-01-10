<?php

class c_cast
{
    private $type;
    private $operand;

    function __construct($type, $operand)
    {
        $this->type = $type;
        $this->operand = $operand;
    }

    function format()
    {
        return '(' . $this->type->format() . ') ' . $this->operand->format();
    }
}
