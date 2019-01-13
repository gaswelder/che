<?php

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
            $self->fieldlists[] = c_struct_fieldlist::parse($lexer);
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
}
