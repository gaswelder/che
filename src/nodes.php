<?php

class c_anonymous_parameters
{
    public $forms = [];
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

class c_ellipsis
{
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
}

class c_enum
{
    public $members = [];
    public $pub;
}

class c_form
{
    public $str = '';
}
