<?php

class c_variable_declaration
{
    private $type;
    private $forms = [];
    private $values = [];

    static function parse($lexer)
    {
        $self = new self;
        $self->type = c_type::parse($lexer);

        $self->forms[] = c_form::parse($lexer);
        expect($lexer, '=', 'variable declaration');
        $self->values[] = parse_expression($lexer);

        while ($lexer->follows(',')) {
            $lexer->get();
            $self->forms[] = c_form::parse($lexer);
            expect($lexer, '=', 'variable declaration');
            $self->values[] = parse_expression($lexer);
        }

        expect($lexer, ';');
        return $self;
    }

    function format()
    {
        $s = $this->type->format() . ' ';
        foreach ($this->forms as $i => $form) {
            $value = $this->values[$i];
            if ($i > 0) $s .= ', ';
            $s .= $form->format() . ' = ' . $value->format();
        }
        $s .= ";\n";
        return $s;
    }
}
