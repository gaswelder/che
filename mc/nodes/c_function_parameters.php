<?php

class c_function_parameters
{
    private $parameters = [];

    static function parse($lexer)
    {
        $self = new self;

        expect($lexer, '(');
        if (!$lexer->follows(')')) {
            $self->parameters[] = c_function_parameter::parse($lexer);
            while ($lexer->follows(',')) {
                $lexer->get();
                if ($lexer->follows('...')) {
                    $lexer->get();
                    $self->parameters[] = new c_ellipsis();
                    break;
                }
                $self->parameters[] = c_function_parameter::parse($lexer);
            }
        }
        expect($lexer, ')');
        return $self;
    }

    function format()
    {
        $s = '(';
        foreach ($this->parameters as $i => $parameter) {
            if ($i > 0) $s .= ', ';
            $s .= $parameter->format();
        }
        $s .= ')';
        return $s;
    }

    function translate()
    {
        $compat = new self;
        foreach ($this->parameters as $parameter) {
            if ($parameter instanceof c_ellipsis) {
                $compat->parameters[] = $parameter;
                continue;
            }
            $compat->parameters = array_merge($compat->parameters, $parameter->translate());
        }
        return $compat;
    }
}
