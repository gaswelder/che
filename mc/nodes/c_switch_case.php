<?php

class c_switch_case
{
    private $value;
    private $statements = [];

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, 'case');
        if ($lexer->follows('word')) {
            $self->value = c_identifier::parse($lexer);
        } else {
            $self->value = c_literal::parse($lexer);
        }
        expect($lexer, ':');
        $until = ['case', 'break', 'default', '}'];
        while ($lexer->more() && !in_array($lexer->peek()->type, $until)) {
            $self->statements[] = parse_statement($lexer);
        }
        return $self;
    }

    function format()
    {
        $s = 'case ' . $this->value->format() . ":\n";
        foreach ($this->statements as $statement) {
            $s .= $statement->format() . ";\n";
        }
        return $s;
    }
}
