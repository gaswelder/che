<?php

class c_struct_fieldlist
{
    private $type;
    private $forms = [];

    static function parse($lexer)
    {
        $self = new self;
        if ($lexer->follows('struct')) {
            throw new Exception("can't parse nested structs, please consider a typedef");
        }
        $self->type = c_type::parse($lexer);
        $self->forms[] = c_form::parse($lexer);

        while ($lexer->follows(',')) {
            $lexer->get();
            $self->forms[] = c_form::parse($lexer);
        }

        expect($lexer, ';');
        return $self;
    }

    function format()
    {
        $s = $this->type->format() . ' ';
        foreach ($this->forms as $i => $form) {
            if ($i > 0) $s .= ', ';
            $s .= $form->format();
        }
        $s .= ';';
        return $s;
    }
}