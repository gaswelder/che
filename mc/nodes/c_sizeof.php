<?php

class c_sizeof
{
    private $argument;

    static function parse($lexer)
    {
        expect($lexer, 'sizeof');
        expect($lexer, '(');
        $self = new self;
        $self->argument = c_form::parse($lexer);
        expect($lexer, ')');
        return $self;
    }

    function format()
    {
        return 'sizeof(' . $this->argument->format() . ')';
    }
}
