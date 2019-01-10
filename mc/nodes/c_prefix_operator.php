<?php

class c_prefix_operator
{
    private $operand;
    private $operator;

    function __construct($operator, $operand)
    {
        $this->operator = $operator;
        $this->operand = $operand;
    }

    function format()
    {
        return $this->operator . $this->operand->format();
    }
}
