<?php

class c_loop_counter_declaration
{
    public $type;
    public $name;
    public $value;

    function format()
    {
        $s = sprintf(
            '%s %s = %s',
            $this->type->format(),
            $this->name->format(),
            $this->value->format()
        );
        return $s;
    }
}
