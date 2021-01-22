<?php

class c_type
{
    public $type;
    public $const = false;

    static function make($name)
    {
        $self = new self;
        $self->type = $name;
        return $self;
    }

    function format()
    {
        $s = '';
        if ($this->const) {
            $s .= 'const ';
        }
        $s .= $this->type;
        return $s;
    }
}
