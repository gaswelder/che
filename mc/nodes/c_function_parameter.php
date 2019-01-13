<?php

class c_function_parameter
{
    private $type;
    private $forms = [];

    static function parse($lexer)
    {
        $self = new self;
        $self->type = c_type::parse($lexer);
        $self->forms[] = c_form::parse($lexer);
        while ($lexer->follows(',') && $lexer->peek(1)->type != 'const' && !is_type($lexer->peek(1), $lexer->typenames)) {
            $lexer->get();
            $self->forms[] = c_form::parse($lexer);
        }
        return $self;
    }

    function format()
    {
        $s = $this->type->format() . ' ';
        foreach ($this->forms as $i => $form) {
            if ($i > 0) $s .= ', ';
            $s .= $form->format();
        }
        return $s;
    }
}
