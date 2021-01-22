<?php

class c_for
{
    public $init;
    public $condition;
    public $action;
    public $body;

    function format()
    {
        return sprintf(
            'for (%s; %s; %s) %s',
            $this->init->format(),
            $this->condition->format(),
            $this->action->format(),
            $this->body->format()
        );
    }
}
