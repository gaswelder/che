<?php

class c_function_arguments
{
    public $arguments = [];

    function format()
    {
        $s = '(';
        foreach ($this->arguments as $i => $argument) {
            if ($i > 0) $s .= ', ';
            $s .= $argument->format();
        }
        $s .= ')';
        return $s;
    }
}
