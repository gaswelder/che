<?php

class c_form
{
    public $str = '';

    function name()
    {
        return trim($this->str, '[]*');
    }

    function format()
    {
        return $this->str;
    }
}
