<?php

class c_prefix_operator
{
    private $operand;
    private $type;

    static function parse($lexer)
    {
        if (!is_prefix_op($lexer->peek())) {
            throw new Exception("prefix operator expected");
        }
        $self = new self;
        $self->type = $lexer->get()->type;
        $self->operand = c_identifier::parse($lexer);
        return $self;
    }

    function format()
    {
        return $this->type . $this->operand->format();
    }
}
