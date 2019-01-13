<?php

class c_form
{
    private $str = '';

    static function parse($lexer)
    {
        // *argv[]
        $self = new self;
        while ($lexer->follows('*')) {
            $self->str .= $lexer->get()->type;
        }
        $self->str .= expect($lexer, 'word')->content;

        if ($lexer->follows('[')) {
            $self->str .= $lexer->get()->type;
            while ($lexer->more() && $lexer->peek()->type != ']') {
                $self->str .= $lexer->get()->content;
            }
            expect($lexer, ']');
            $self->str .= ']';
        }
        return $self;
    }

    function format()
    {
        return $this->str;
    }
}
