<?php

class c_switch_case
{
    public $value;
    public $statements = [];

    function format()
    {
        $s = 'case ' . $this->value->format() . ": {\n";
        foreach ($this->statements as $statement) {
            $s .= $statement->format() . ";\n";
        }
        $s .= "}\n";
        return $s;
    }
}
