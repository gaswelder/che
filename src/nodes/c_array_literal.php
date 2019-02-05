<?php

function parse_array_literal_member($lexer)
{
    if ($lexer->follows('{')) {
        return c_array_literal::parse($lexer);
    }
    if ($lexer->follows('word')) {
        return c_identifier::parse($lexer);
    }
    return c_literal::parse($lexer);
}

class c_array_literal
{
    private $values = [];

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, '{', 'array literal');
        if (!$lexer->follows('}')) {
            $self->values[] = parse_array_literal_member($lexer);
            while ($lexer->follows(',')) {
                $lexer->get();
                $self->values[] = parse_array_literal_member($lexer);
            }
        }
        expect($lexer, '}', 'array literal');
        return $self;
    }

    function format()
    {
        $s = '{';
        foreach ($this->values as $i => $value) {
            if ($i > 0) $s .= ', ';
            $s .= $value->format();
        }
        if (empty($this->values)) {
            $s .= '0';
        }
        $s .= '}';
        return $s;
    }
}
