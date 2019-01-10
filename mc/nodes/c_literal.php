<?php

class c_literal
{
    private $value;
    private $type;

    static function parse($lexer)
    {
        $types = ['string', 'num'];
        $self = new self;
        foreach ($types as $type) {
            if ($lexer->peek()->type != $type) {
                continue;
            }
            $self->value = $lexer->get()->content;
            $self->type = $type;
            return $self;
        }
        throw new Exception("literal expected");
    }

    function format()
    {
        if ($this->type == 'string') {
            return '"' . $this->value . '"';
        }
        return $this->value;
    }
}
