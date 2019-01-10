<?php

class c_literal
{
    private $value;
    private $type;

    static function parse($lexer)
    {
        $types = ['string', 'num', 'char'];
        $self = new self;
        foreach ($types as $type) {
            if ($lexer->peek()->type != $type) {
                continue;
            }
            $self->value = $lexer->get()->content;
            $self->type = $type;
            return $self;
        }
        throw new Exception("literal expected, got " . $lexer->peek());
    }

    function format()
    {
        switch ($this->type) {
            case 'string':
                return '"' . $this->value . '"';
            case 'char':
                return '\'' . $this->value . '\'';
            default:
                return $this->value;
        }
    }
}
