<?php

class c_if
{
    public $condition;
    public $body;
    public $else = null;

    function format()
    {
        $s = sprintf('if (%s) %s', $this->condition->format(), $this->body->format());
        if ($this->else) {
            $s .= ' else ' . $this->else->format();
        }
        return $s;
    }
}
