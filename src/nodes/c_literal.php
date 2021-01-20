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
            if ($lexer->peek()['kind'] != $type) {
                continue;
            }
            $self->value = $lexer->get()['content'];
            $self->type = $type;
            return $self;
        }
        if ($lexer->peek()['kind'] == 'word' && $lexer->peek()['content'] == 'NULL') {
            $lexer->get();
            $self->type = 'null';
            $self->value = 'NULL';
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
