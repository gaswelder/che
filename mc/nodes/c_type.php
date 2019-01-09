<?php

class c_type
{
    private $type;
    private $const = false;

    static function parse($lexer)
    {
        $types = ['const'];

        $self = new self;

        if ($lexer->peek()->type == 'const') {
            $lexer->get();
            $self->const = true;
        }

        $tok = expect($lexer, 'word');
        $self->type = $tok->content;
        return $self;
    }

    function format()
    {
        $s = '';
        if ($this->const) {
            $s .= 'const ';
        }
        $s .= $this->type;
        return $s;
    }
}
