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
        $s = "{\n";
        foreach ($this->statements as $statement) {
            $s .= "\t";
            if (is_array($statement)) {
                foreach ($statement as $atom) {
                    if (get_class($atom) == 'token') {
                        $s .= $atom->type;
                    } else {
                        $s .= $atom->format() . ' ';
                    }
                }
                $s .= ";\n";
                continue;
            }
            $s .= $statement->format() . ";\n";
        }
        $s .= "\n}\n";
        return $s;
    }
}
