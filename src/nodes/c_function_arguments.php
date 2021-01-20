<?php

class c_function_arguments
{
    private $arguments = [];

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, '(');
        if ($lexer->more() && $lexer->peek()['kind'] != ')') {
            $self->arguments[] = parse_expression($lexer);
            while ($lexer->follows(',')) {
                $lexer->get();
                $self->arguments[] = parse_expression($lexer);
            }
        }
        expect($lexer, ')');
        return $self;
    }

    function format()
    {
        $s = '(';
        foreach ($this->arguments as $i => $argument) {
            if ($i > 0) $s .= ', ';
            $s .= $argument->format();
        }
        $s .= ')';
        return $s;
    }
}
