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

        if ($lexer->follows('{')) {
            $self->type = c_composite_type::parse($lexer);
            $self->alias = c_identifier::parse($lexer);
            expect($lexer, ';', 'typedef');
            return $self;
        }

        $self->type = c_type::parse($lexer, 'typedef');
        while ($lexer->follows('*')) {
            $self->before .= $lexer->get()['kind'];
        }
        $self->alias = c_identifier::parse($lexer);

        if ($lexer->follows('(')) {
            $self->after .= c_anonymous_parameters::parse($lexer)->format();
        }

        if ($lexer->follows('[')) {
            $lexer->get();
            $self->after .= '[';
            $self->after .= expect($lexer, 'num')['content'];
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

    function translate()
    {
        if ($this->type instanceof c_composite_type) {
            $struct_name = '__' . $this->name() . '_struct';
            $typedef = new self;
            $typedef->alias = $this->alias;
            $typedef->type = c_type::make('struct ' . $struct_name);
            return [
                new c_compat_struct_forward_declaration(c_identifier::make($struct_name)),
                new c_compat_struct_definition($struct_name, $this->type),
                $typedef
            ];
        }
        return [$this];
    }
}
