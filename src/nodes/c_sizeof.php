<?php

class c_sizeof
{
    public $argument;

    function format()
    {
        return 'sizeof(' . $this->argument->format() . ')';
    }
}
