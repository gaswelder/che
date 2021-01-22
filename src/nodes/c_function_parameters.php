<?php

class c_function_parameters
{
    public $parameters = [];

    function format()
    {
        $s = '(';
        foreach ($this->parameters as $i => $parameter) {
            if ($i > 0) $s .= ', ';
            $s .= $parameter->format();
        }
        $s .= ')';
        return $s;
    }

    function translate()
    {
        $compat = new self;
        foreach ($this->parameters as $parameter) {
            if ($parameter instanceof c_ellipsis) {
                $compat->parameters[] = $parameter;
                continue;
            }
            $compat->parameters = array_merge($compat->parameters, $parameter->translate());
        }
        return $compat;
    }
}
