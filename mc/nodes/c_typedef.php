<?php

class c_typedef
{
    private $type;
    private $before = '';
    private $alias;
    private $after = '';

    static function parse($lexer)
    {
        $self = new self;

        expect($lexer, 'typedef');
        $self->type = c_type::parse($lexer, 'typedef');
        while ($lexer->follows('*')) {
            $self->before .= $lexer->get()->type;
        }
        $self->alias = c_identifier::parse($lexer);

        if ($lexer->follows('(')) {
            $self->after .= c_anonymous_parameters::parse($lexer)->format();
        }

        if ($lexer->follows('[')) {
            $lexer->get();
            $self->after .= '[';
            $self->after .= expect($lexer, 'num')->content;
            expect($lexer, ']');
            $self->after .= ']';
        }

        expect($lexer, ';', 'typedef');
        return $self;
    }

    function format()
    {
        return 'typedef ' . $this->type->format() . ' '
            . $this->before
            . $this->alias->format()
            . $this->after
            . ";\n";
    }

    function name()
    {
        return $this->alias->format();
    }
}
