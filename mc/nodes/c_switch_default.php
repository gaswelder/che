<?php

class c_switch_default
{
    private $statements = [];

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, 'default');
        while ($lexer->more() && $lexer->peek()->type != '}') {
            $self->statements[] = parse_statement($lexer);
        }
        return $self;
    }

    function format()
    {
        $s = "default:\n";
        foreach ($this->statements as $statement) {
            $s .= $statement->format() . "\n";
        }
        return $s;
    }
}
