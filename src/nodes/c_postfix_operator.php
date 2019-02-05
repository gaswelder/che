<?php

class c_postfix_operator
{
    private $operand;
    private $operator;

    function __construct($operand, $operator)
    {
        $this->operand = $operand;
        $this->operator = $operator;
    }

    function format()
    {
        return $this->operand->format() . $this->operator;
    }
}
