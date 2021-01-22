<?php



class c_while
{
    public $condition;
    public $body;

    function format()
    {
        return sprintf('while (%s) %s', $this->condition->format(), $this->body->format());
    }
}
