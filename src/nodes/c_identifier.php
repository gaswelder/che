<?php

class c_identifier
{
    public $name;

    static function make(string $name)
    {
        $self = new self;
        $self->name = $name;
        return $self;
    }

    function format()
    {
        return $this->name;
    }
}
