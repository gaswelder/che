<?php

class c_switch_default
{
    public $statements = [];

    function format()
    {
        $s = "default: {\n";
        foreach ($this->statements as $statement) {
            $s .= $statement->format() . ";\n";
        }
        $s .= "}\n";
        return $s;
    }
}
