<?php

class c_struct_literal
{
    private $members = [];

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, '{', 'struct literal');
        $self->members[] = c_struct_literal_member::parse($lexer);
        while ($lexer->follows(',')) {
            $lexer->get();
            $self->members[] = c_struct_literal_member::parse($lexer);
        }
        expect($lexer, '}', 'struct literal');
        return $self;
    }

    function format()
    {
        $s = "{\n";
        foreach ($this->members as $member) {
            $s .= "\t" . $member->format() . ",\n";
        }
        $s .= "}\n";
        return $s;
    }
}
