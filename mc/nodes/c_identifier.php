<?php

class c_identifier
{
    private $name;

    static function parse($lexer)
    {
        $tok = expect($lexer, 'word', 'identifier');
        $self = new self;
        $self->name = $tok->content;
        return $self;
    }

    function format()
    {
        return $this->name;
    }
}
