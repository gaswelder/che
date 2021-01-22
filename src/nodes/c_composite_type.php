<?php

class c_composite_type
{
    public $fieldlists = [];

    function format()
    {
        $s = '';
        foreach ($this->fieldlists as $fieldlist) {
            $s .= $fieldlist->format() . "\n";
        }
        return "{\n"
            . indent($s) . "}";
    }
}
