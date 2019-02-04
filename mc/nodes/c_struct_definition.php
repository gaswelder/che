<?php

class c_compat_struct_forward_declaration
{
    private $name;

    function __construct($name)
    {
        $this->name = $name;
    }

    function format()
    {
        return 'struct ' . $this->name->format() . ";\n";
    }
}

class c_struct_definition
{
    private $name;
    private $fieldlists = [];

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, 'struct');
        $self->name = c_identifier::parse($lexer);
        expect($lexer, '{', 'struct definition');
        while ($lexer->more() && $lexer->peek()->type != '}') {
            if ($lexer->peek()->type == 'union') {
                $self->fieldlists[] = c_union::parse($lexer);
            } else {
                $self->fieldlists[] = c_struct_fieldlist::parse($lexer);
            }
        }
        expect($lexer, '}');
        expect($lexer, ';');
        return $self;
    }

    function format()
    {
        $s = '';
        foreach ($this->fieldlists as $fieldlist) {
            $s .= $fieldlist->format() . "\n";
        }
        return 'struct ' . $this->name->format() . " {\n"
            . indent($s) . "};\n\n";
    }

    function translate()
    {
        return [
            new c_compat_struct_forward_declaration($this->name),
            $this
        ];
    }
}
