<?php

class c_form
{
    private $str = '';

    static function parse($lexer)
    {
        // *argv[]
        $self = new self;
        while ($lexer->follows('*')) {
            $self->str .= $lexer->get()['kind'];
        }
        $self->str .= expect($lexer, 'word')['content'];

        while ($lexer->follows('[')) {
            $self->str .= $lexer->get()['kind'];
            while ($lexer->more() && $lexer->peek()['kind'] != ']') {
                $expr = parse_expression($lexer);
                $self->str .= $expr->format();
            }
            expect($lexer, ']');
            $self->str .= ']';
        }
        return $self;
    }

    function name()
    {
        return trim($this->str, '[]*');
    }

    function format()
    {
        return $this->str;
    }
}
