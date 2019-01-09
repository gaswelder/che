<?php

class c_function_parameters
{
    private $parameters = [];

    static function parse($lexer)
    {
        expect($lexer, '(');
        $self = new self;
        if ($lexer->more() && $lexer->peek()->type != ')') {
            $self->parameters[] = c_function_parameter::parse($lexer);
        }
        while ($lexer->more() && $lexer->peek()->type == ',') {
            $lexer->get();
            $self->parameters[] = c_function_parameter::parse($lexer);
        }
        expect($lexer, ')');
        return $self;
    }

    function format()
    {
        $s = '(';
        foreach ($this->parameters as $i => $parameter) {
            if ($i > 0) $s .= ', ';
            $s .= $parameter->format();
        }
        $s .= ')';
        return $s;
    }
}
