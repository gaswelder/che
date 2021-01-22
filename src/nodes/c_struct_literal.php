<?php

class c_struct_literal
{
    public $members = [];

    function format()
    {
        $s = "{\n";
        foreach ($this->members as $member) {
            $s .= "\t" . $member->format() . ",\n";
        }
        $s .= "}\n";
        return $s;
    }
}
