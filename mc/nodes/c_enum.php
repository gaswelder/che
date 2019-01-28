<?php

class c_enum_member
{
    private $id;
    private $value;

    static function parse($lexer)
    {
        $self = new self;
        $self->id = c_identifier::parse($lexer, 'enum member');
        if ($lexer->follows('=')) {
            $lexer->get();
            $self->value = c_literal::parse($lexer, 'enum member');
        }
        return $self;
    }

    function format()
    {
        $s = $this->id->format();
        if ($this->value) {
            $s .= ' = ' . $this->value->format();
        }
        return $s;
    }
}

class c_enum
{
    private $members = [];

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, 'enum', 'enum definition');
        expect($lexer, '{', 'enum definition');
        $self->members[] = c_enum_member::parse($lexer);
        while ($lexer->follows(',')) {
            $lexer->get();
            $self->members[] = c_enum_member::parse($lexer);
        }
        expect($lexer, '}', 'enum definition');
        expect($lexer, ';', 'enum definition');
        return $self;
    }

    function format()
    {
        $s = "enum {\n";
        foreach ($this->members as $id) {
            $s .= "\t" . $id->format() . ",\n";
        }
        $s .= "}\n";
        return $s;
    }
}