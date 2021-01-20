<?php

class c_return
{
    private $expression;

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, 'return');
        if ($lexer->peek()['kind'] == ';') {
            $lexer->get();
            return $self;
        }
        $self->expression = parse_expression($lexer);
        expect($lexer, ';');
        return $self;
    }

    function format()
    {
        if (!$this->expression) {
            return 'return;';
        }
        return 'return ' . $this->expression->format() . ';';
    }
}
