<?php

class c_typedef
{
    public $type;
    public $before = '';
    public $alias;
    public $after = '';

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
