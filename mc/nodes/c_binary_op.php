<?php

class c_binary_op
{
    private $op;
    private $a;
    private $b;

    function __construct($op, $a, $b)
    {
        $this->op = $op;
        $this->a = $a;
        $this->b = $b;
    }

    function format()
    {
        $parts = [
            $this->brace_if_needed($this->a),
            $this->op,
            $this->brace_if_needed($this->b)
        ];
        if (in_array($this->op, ['.', '->'])) {
            return implode('', $parts);
        }
        return implode(' ', $parts);
    }

    private function brace_if_needed($operand)
    {
        if ($operand instanceof c_binary_op && operator_strength($operand->op) < operator_strength($this->op)) {
            return '(' . $operand->format() . ')';
        }
        return $operand->format();
    }
}
