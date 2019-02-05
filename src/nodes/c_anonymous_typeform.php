<?php

class c_anonymous_typeform
{
    private $type;
    private $ops = [];

    static function parse($lexer)
    {
        $self = new self;
        $self->type = c_type::parse($lexer);
        while ($lexer->follows('*')) {
            $self->ops[] = $lexer->get()->type;
        }
        return $self;
    }

    function format()
    {
        $s = $this->type->format();
        foreach ($this->ops as $op) {
            $s .= $op;
        }
        return $s;
    }
}
