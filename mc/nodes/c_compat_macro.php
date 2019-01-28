<?php

class c_compat_macro
{
    private $content;

    static function parse($lexer)
    {
        $self = new self;
        $self->content = expect($lexer, 'macro')->content;
        return $self;
    }

    function format()
    {
        return $this->content . "\n";
    }
}
