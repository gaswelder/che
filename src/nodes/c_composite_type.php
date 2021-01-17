<?php

class c_composite_type
{
    private $fieldlists = [];

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, '{', 'struct type definition');
        while ($lexer->more() && $lexer->peek()['type'] != '}') {
            if ($lexer->peek()['type'] == 'union') {
                $self->fieldlists[] = c_union::parse($lexer);
            } else {
                $self->fieldlists[] = c_struct_fieldlist::parse($lexer);
            }
        }
        expect($lexer, '}');
        return $self;
    }

    function format()
    {
        $s = '';
        foreach ($this->fieldlists as $fieldlist) {
            $s .= $fieldlist->format() . "\n";
        }
        return "{\n"
            . indent($s) . "}";
    }
}
