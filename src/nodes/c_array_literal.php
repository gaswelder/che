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

class c_array_literal_entry
{
    private $index = null;
    private $value;

    static function parse($lexer)
    {
        $self = new self;
        if ($lexer->follows('[')) {
            $lexer->get();
            if ($lexer->follows('word')) {
                $self->index = c_identifier::parse($lexer);
            } else {
                $self->index = c_literal::parse($lexer);
            }
            expect($lexer, ']', 'array literal entry');
            expect($lexer, '=', 'array literal entry');
        }
        $self->value = parse_array_literal_member($lexer);
        return $self;
    }

    function format()
    {
        $s = '';
        if ($this->index) {
            $s .= '[' . $this->index->format() . '] = ';
        }
        $s .= $this->value->format();
        return $s;
    }
}

class c_array_literal
{
    private $values = [];

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, '{', 'array literal');
        if (!$lexer->follows('}')) {
            $self->values[] = c_array_literal_entry::parse($lexer);
            while ($lexer->follows(',')) {
                $lexer->get();
                $self->values[] = c_array_literal_entry::parse($lexer);
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
