<?php

class c_array_index
{
    private $array;
    private $index;

    function __construct($array, $index)
    {
        $this->array = $array;
        $this->index = $index;
    }

    function format()
    {
        return $this->array->format() . '[' . $this->index . ']';
    }
}
