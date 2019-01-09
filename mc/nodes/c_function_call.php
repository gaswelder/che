<?php

class c_function_call
{
    private $name;
    private $arguments = [];

    static function parse($lexer)
    {
        // For example: fatal("Unknown argument: %s", *argv)
        $self = new self;
        $self->name = c_identifier::parse($lexer);
        expect($lexer, '(');
        if ($lexer->more() && $lexer->peek()->type != ')') {
            $self->arguments[] = parse_expression($lexer);
            while ($lexer->more() && $lexer->peek()->type == ',') {
                $lexer->get();
                $self->arguments[] = parse_expression($lexer);
            }
        }
        expect($lexer, ')');
        return $self;
    }

    function format()
    {
        $s = $this->name->format() . '(';
        foreach ($this->arguments as $i => $argument) {
            if ($i > 0) $s .= ', ';
            $s .= format_expression($argument);
        }
        $s .= ')';
        return $s;
    }
}
