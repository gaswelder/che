<?php

class c_sizeof
{
    private $argument;

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, 'sizeof');
        expect($lexer, '(');
        if (is_type($lexer->peek(), $lexer->typenames)) {
            $self->argument = c_type::parse($lexer);
        } else {
            $self->argument = parse_expression($lexer);
        }
        expect($lexer, ')');
        return $self;
    }

    function format()
    {
        return 'sizeof(' . $this->argument->format() . ')';
    }
}