<?php

class c_anonymous_parameters
{
    public $forms = [];
}

class c_anonymous_typeform
{
    public $type;
    public $ops = [];
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

    public function brace_if_needed($operand)
    {
        if ($operand instanceof c_binary_op && operator_strength($operand->op) < operator_strength($this->op)) {
            return '(' . format_node($operand) . ')';
        }
        return format_node($operand);
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

class c_compat_macro
{
    public $content;

    public function kv()
    {
        $pos = strpos($this->content, ' ');
        if ($pos === false) {
            throw new Exception("can't get macro name from '$this->content'");
        }
        return [
            substr($this->content, 1, $pos - 1),
            substr($this->content, $pos + 1)
        ];
    }

    function name()
    {
        return $this->kv()[0];
    }

    function value()
    {
        return $this->kv()[1];
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

    function synopsis()
    {
        $elements = [];

        $exports = [
            c_typedef::class,
            c_compat_struct_definition::class,
            c_compat_struct_forward_declaration::class,
            c_compat_macro::class,
        ];

        foreach ($this->elements as $element) {
            if ($element instanceof c_compat_function_declaration && !$element->is_static()) {
                $elements[] = $element->forward_declaration();
                continue;
            }
            if (in_array(get_class($element), $exports)) {
                $elements[] = $element;
                continue;
            }
            if ($element instanceof c_compat_enum && !$element->is_private()) {
                $elements[] = $element;
                continue;
            }
        }
        return $elements;
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

    function translate()
    {
        return new c_compat_enum($this->members, !$this->pub);
    }
}

class c_form
{
    public $str = '';

    function name()
    {
        return trim($this->str, '[]*');
    }
}

class c_for
{
    public $init;
    public $condition;
    public $action;
    public $body;
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

    function translate()
    {
        return new c_compat_function_declaration(
            !$this->pub,
            $this->type,
            $this->form,
            $this->parameters->translate(),
            $this->body
        );
    }
}

class c_function_parameter
{
    public $type;
    public $forms = [];

    function translate()
    {
        $compat = [];
        foreach ($this->forms as $form) {
            $p = new self;
            $p->type = $this->type;
            $p->forms = [$form];
            $compat[] = $p;
        }
        return $compat;
    }
}

class c_function_parameters
{
    public $parameters = [];

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

class c_identifier
{
    public $name;

    static function make(string $name)
    {
        $self = new self;
        $self->name = $name;
        return $self;
    }
}

class c_if
{
    public $condition;
    public $body;
    public $else = null;
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

class c_loop_counter_declaration
{
    public $type;
    public $name;
    public $value;
}

class c_module
{
    public $elements = [];

    function json()
    {
        return ['type' => 'c_module', 'elements' => array_map(function ($element) {
            return $element->json();
        }, $this->elements)];
    }

    function imports()
    {
        $list = [];
        foreach ($this->elements as $element) {
            if ($element instanceof c_import) {
                $list[] = $element;
            }
        }
        return $list;
    }

    function types()
    {
        $list = [];
        foreach ($this->elements as $element) {
            if ($element instanceof c_typedef) {
                $list[] = $element->name();
            }
        }
        return $list;
    }

    function elements()
    {
        return $this->elements;
    }

    function merge(c_module $that)
    {
        $result = new self;
        $result->elements = array_merge($this->elements, $that->elements);
        return $result;
    }
}

class c_module_variable
{
    public $type;
    public $form;
    public $value;

    function __construct($type, $form, $value)
    {
        $this->type = $type;
        $this->form = $form;
        $this->value = $value;
    }
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

class c_prefix_operator
{
    public $operand;
    public $operator;

    function __construct($operator, $operand)
    {
        $this->operator = $operator;
        $this->operand = $operand;
    }
}

class c_return
{
    public $expression;
}

class c_sizeof
{
    public $argument;
}

class c_struct_fieldlist
{
    public $type;
    public $forms = [];
}

class c_struct_literal_member
{
    public $name;
    public $value;
}

class c_struct_literal
{
    public $members = [];
}

class c_switch_case
{
    public $value;
    public $statements = [];
}

class c_switch_default
{
    public $statements = [];
}

class c_switch
{
    public $value;
    public $cases = [];
    public $default;
}

class c_typedef
{
    public $type;
    public $before = '';
    public $alias;
    public $after = '';

    function name()
    {
        return format_node($this->alias);
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

class c_type
{
    public $type;
    public $const = false;

    static function make($name)
    {
        $self = new self;
        $self->type = $name;
        return $self;
    }
}

class c_union_field
{
    public $type;
    public $form;
}

class c_union
{
    public $form;
    public $fields = [];
}

class c_variable_declaration
{
    public $type;
    public $forms = [];
    public $values = [];
}

class c_while
{
    public $condition;
    public $body;
}
