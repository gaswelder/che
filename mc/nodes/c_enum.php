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

class c_compat_enum
{
    private $hidden;
    private $members;

    function __construct($members, $hidden)
    {
        $this->members = $members;
        $this->hidden = $hidden;
    }

    function is_private()
    {
        return $this->hidden;
    }

    function format()
    {
        $s = "enum {\n";
        foreach ($this->members as $i => $member) {
            if ($i > 0) {
                $s .= ",\n";
            }
            $s .= "\t" . $member->format();
        }
        $s .= "\n};\n";
        return $s;
    }
}

class c_enum
{
    private $members = [];
    private $pub;

    static function parse($lexer, $pub)
    {
        $self = new self;
        $self->pub = $pub;
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
        $s = '';
        if ($this->pub) {
            $s .= 'pub ';
        }
        $s .= "enum {\n";
        foreach ($this->members as $id) {
            $s .= "\t" . $id->format() . ",\n";
        }
        $s .= "}\n";
        return $s;
    }

    function translate()
    {
        return new c_compat_enum($this->members, !$this->pub);
    }
}