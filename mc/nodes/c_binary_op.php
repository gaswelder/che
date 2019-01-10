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
        return sprintf('(%s) %s (%s)', $this->a->format(), $this->op, $this->b->format());
    }
}
