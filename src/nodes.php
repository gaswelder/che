<?php

class c_anonymous_parameters
{
    public $forms = [];
}

class c_array_index
{
    public $array;
    public $index;

    function __construct($array, $index)
    {
        $this->array = $array;
        $this->index = $index;
    }
}

class c_array_literal_entry
{
    public $index = null;
    public $value;
}
class c_array_literal
{
    public $values = [];
}

class c_binary_op
{
    public $op;
    public $a;
    public $b;

    function __construct($op, $a, $b)
    {
        $this->op = $op;
        $this->a = $a;
        $this->b = $b;
    }
}

class c_body
{
    public $statements = [];
}

class c_cast
{
    public $type;
    public $operand;

    function __construct($type, $operand)
    {
        $this->type = $type;
        $this->operand = $operand;
    }
}

class c_compat_function_declaration
{
    public $static;
    public $type;
    public $form;
    public $parameters;
    public $body;

    function __construct($static, $type, $form, $parameters, $body)
    {
        $this->static = $static;
        $this->type = $type;
        $this->form = $form;
        $this->parameters = $parameters;
        $this->body = $body;
    }

    function forward_declaration()
    {
        return new c_compat_function_forward_declaration($this->static, $this->type, $this->form, $this->parameters);
    }

    function is_static()
    {
        return $this->static;
    }
}

class c_compat_function_forward_declaration
{
    public $static;
    public $type;
    public $form;
    public $parameters;

    function __construct($static, $type, $form, $parameters)
    {
        $this->static = $static;
        $this->type = $type;
        $this->form = $form;
        $this->parameters = $parameters;
    }
}

class c_compat_include
{
    public $name;

    function __construct($name)
    {
        $this->name = $name;
    }
}

class c_compat_module
{
    public $elements = [];
    public $link = [];

    function __construct($elements, $link)
    {
        $this->elements = $elements;
        $this->link = $link;
    }

    function link()
    {
        return $this->link;
    }
}

class c_compat_struct_definition
{
    public $name;
    public $fields;

    function __construct(string $name, c_composite_type $fields)
    {
        $this->name = $name;
        $this->fields = $fields;
    }
}

class c_compat_struct_forward_declaration
{
    public $name;

    function __construct($name)
    {
        $this->name = $name;
    }
}

class c_composite_type
{
    public $fieldlists = [];
}

class c_defer
{
    public $expression;
}

class c_ellipsis
{
}

class c_enum_member
{
    public $id;
    public $value;
}
class c_compat_enum
{
    public $hidden;
    public $members;

    function __construct($members, $hidden)
    {
        $this->members = $members;
        $this->hidden = $hidden;
    }

    function is_private()
    {
        return $this->hidden;
    }
}

class c_enum
{
    public $members = [];
    public $pub;
}

class c_form
{
    public $str = '';

    function name()
    {
        return trim($this->str, '[]*');
    }
}

class c_function_arguments
{
    public $arguments = [];
}

class c_function_call
{
    public $function;
    public $arguments;

    function __construct($function, $arguments)
    {
        $this->function = $function;
        $this->arguments = $arguments;
    }
}

class c_function_declaration
{
    public $pub;
    public $type;
    public $form;
    public $parameters;
    public $body;

    function __construct($pub, $type, $form, $parameters, $body)
    {
        $this->pub = $pub;
        $this->type = $type;
        $this->form = $form;
        $this->parameters = $parameters;
        $this->body = $body;
    }
}

class c_function_parameter
{
    public $type;
    public $forms = [];
}

class c_function_parameters
{
    public $parameters = [];
}

class c_import
{
    public $path;

    function name()
    {
        return $this->path;
    }
}

class c_literal
{
    public $value;
    public $type;
}

class c_module
{
    public $elements = [];
}

class c_postfix_operator
{
    public $operand;
    public $operator;

    function __construct($operand, $operator)
    {
        $this->operand = $operand;
        $this->operator = $operator;
    }
}
