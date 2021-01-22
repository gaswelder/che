<?php

class c_anonymous_typeform
{
    public $type;
    public $ops = [];

    function format()
    {
        $s = $this->type->format();
        foreach ($this->ops as $op) {
            $s .= $op;
        }
        return $s;
    }
}
