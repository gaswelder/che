<?php

class c_body
{
    private $statements = [];

    static function parse($lexer)
    {
        $self = new self;
        if ($lexer->follows('{')) {
            expect($lexer, '{');
            while (!$lexer->follows('}')) {
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
        return "{\n" . indent($s) . "}\n";
    }
}
