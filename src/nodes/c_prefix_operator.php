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
        if ($this->operand instanceof c_binary_op || $this->operand instanceof c_cast) {
            return $this->operator . '(' . $this->operand->format() . ')';
        }
        return $this->operator . $this->operand->format();
    }
}
