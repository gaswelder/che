<?php

class c_union
{
    private $form;
    private $fields = [];

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, 'union');
        expect($lexer, '{');
        while (!$lexer->follows('}')) {
            $self->fields[] = c_union_field::parse($lexer);
        }
        expect($lexer, '}');
        $self->form = c_form::parse($lexer);
        expect($lexer, ';');
        return $self;
    }

    function format()
    {
        $s = '';
        foreach ($this->fields as $field) {
            $s .= $field->format() . "\n";
        }
        return sprintf("union {\n%s\n} %s;\n", indent($s), $this->form->format());
    }
}
