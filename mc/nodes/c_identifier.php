<?php

class c_identifier
{
    private $name;

    static function parse($lexer, $hint = null)
    {
        $tok = expect($lexer, 'word', $hint);
        $self = new self;
        $self->name = $tok->content;
        return $self;
    }

    function format()
    {
        return $this->name;
    }
}
