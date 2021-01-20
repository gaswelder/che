<?php

class c_switch_default
{
    private $statements = [];

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, 'default');
        expect($lexer, ':');
        while ($lexer->more() && $lexer->peek()['kind'] != '}') {
            $self->statements[] = parse_statement($lexer);
        }
        return $self;
    }

    function format()
    {
        $s = "default: {\n";
        foreach ($this->statements as $statement) {
            $s .= $statement->format() . ";\n";
        }
        $s .= "}\n";
        return $s;
    }
}
