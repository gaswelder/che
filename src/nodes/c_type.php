<?php

class c_type
{
    private $type;
    private $const = false;

    static function parse($lexer, $comment = null)
    {
        $self = new self;

        if ($lexer->follows('const')) {
            $lexer->get();
            $self->const = true;
        }

        if ($lexer->follows('struct')) {
            $lexer->get();
            $name = expect($lexer, 'word')->content;
            $self->type = 'struct ' . $name;
        } else {
            $tok = expect($lexer, 'word', $comment);
            $self->type = $tok->content;
        }

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
