<?php

class c_body
{
    private $statements = [];

    static function parse($lexer)
    {
        $self = new self;
        if ($lexer->peek()->type == '{') {
            expect($lexer, '{');
            while ($lexer->more() && $lexer->peek()->type != '}') {
                $self->statements[] = parse_statement($lexer);
            }
            expect($lexer, '}');
        } else {
            $self->statements[] = parse_statement($lexer);
        }
        return $self;
    }

    function format()
    {
        $s = '';
        foreach ($this->statements as $statement) {
            $s .= $statement->format() . ";\n";
        }
        return "{\n" . indent($s) . "}";
    }
}
