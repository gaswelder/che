<?php

class c_anonymous_parameters
{
    private $forms = [];

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, '(', 'anonymous function parameters');
        if (!$lexer->follows(')')) {
            $self->forms[] = c_anonymous_typeform::parse($lexer);
            while ($lexer->follows(',')) {
                $lexer->get();
                $self->forms[] = c_anonymous_typeform::parse($lexer);
            }
        }
        expect($lexer, ')', 'anonymous function parameters');
        return $self;
    }

    function format()
    {
        $s = '(';
        foreach ($this->forms as $i => $form) {
            if ($i > 0) $s .= ', ';
            $s .= $form->format();
        }
        $s .= ')';
        return $s;
    }
}
