<?php

class c_identifier
{
    private $name;

    static function make(string $name)
    {
        $self = new self;
        $self->name = $name;
        return $self;
    }

    static function parse($lexer, $hint = null)
    {
        $tok = expect($lexer, 'word', $hint);
        $self = new self;
        $self->name = $tok['content'];
        return $self;
    }

    function format()
    {
        return $this->name;
    }
}
