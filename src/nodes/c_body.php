<?php

class c_body
{
    public $statements = [];

    function format()
    {
        $s = '';
        foreach ($this->statements as $statement) {
            $s .= $statement->format() . ";\n";
        }
        return "{\n" . indent($s) . "}\n";
    }
}
