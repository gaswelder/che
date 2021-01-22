<?php

class c_anonymous_parameters
{
    public $forms = [];

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

class c_anonymous_typeform
{
    public $type;
    public $ops = [];

    function format()
    {
        $s = $this->type->format();
        foreach ($this->ops as $op) {
            $s .= $op;
        }
        return $s;
    }
}

class c_array_index
{
    private $array;
    private $index;

    function __construct($array, $index)
    {
        $this->array = $array;
        $this->index = $index;
    }

    function format()
    {
        return $this->array->format() . '[' . $this->index->format() . ']';
    }
}

class c_array_literal_entry
{
    public $index = null;
    public $value;

    function format()
    {
        $s = '';
        if ($this->index) {
            $s .= '[' . $this->index->format() . '] = ';
        }
        $s .= $this->value->format();
        return $s;
    }
}
class c_array_literal
{
    public $values = [];

    function format()
    {
        $s = '{';
        foreach ($this->values as $i => $value) {
            if ($i > 0) $s .= ', ';
            $s .= $value->format();
        }
        if (empty($this->values)) {
            $s .= '0';
        }
        $s .= '}';
        return $s;
    }
}

class c_binary_op
{
    private $op;
    private $a;
    private $b;

    function __construct($op, $a, $b)
    {
        $this->op = $op;
        $this->a = $a;
        $this->b = $b;
    }

    function format()
    {
        $parts = [
            $this->brace_if_needed($this->a),
            $this->op,
            $this->brace_if_needed($this->b)
        ];
        if (in_array($this->op, ['.', '->'])) {
            return implode('', $parts);
        }
        return implode(' ', $parts);
    }

    private function brace_if_needed($operand)
    {
        if ($operand instanceof c_binary_op && operator_strength($operand->op) < operator_strength($this->op)) {
            return '(' . $operand->format() . ')';
        }
        return $operand->format();
    }
}

class c_body
{
    public $statements = [];

    function format()
    {
        $s = '';
        foreach ($this->statements as $statement) {
            $s .= $statement->format() . ";\n";
        }
        return "{\n" . indent($s) . "}\n";
    }
}

class c_cast
{
    private $type;
    private $operand;

    function __construct($type, $operand)
    {
        $this->type = $type;
        $this->operand = $operand;
    }

    function format()
    {
        return '(' . $this->type->format() . ') ' . $this->operand->format();
    }
}

class c_compat_function_declaration
{
    private $static;
    private $type;
    private $form;
    private $parameters;
    private $body;

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

    function format()
    {
        $s = '';
        if ($this->static && $this->form->format() != 'main') {
            $s .= 'static ';
        }
        $s .= $this->type->format()
            . ' ' . $this->form->format()
            . $this->parameters->format()
            . ' ' . $this->body->format();
        return $s;
    }
}

class c_compat_function_forward_declaration
{
    private $static;
    private $type;
    private $form;
    private $parameters;

    function __construct($static, $type, $form, $parameters)
    {
        $this->static = $static;
        $this->type = $type;
        $this->form = $form;
        $this->parameters = $parameters;
    }

    function format()
    {
        $s = '';
        if ($this->static && $this->form->format() != 'main') {
            $s .= 'static ';
        }
        $s .= $this->type->format()
            . ' ' . $this->form->format()
            . $this->parameters->format() . ";\n";
        return $s;
    }
}

class c_compat_include
{
    private $name;

    function __construct($name)
    {
        $this->name = $name;
    }

    function format()
    {
        return "#include $this->name\n";
    }
}

class c_compat_macro
{
    public $content;

    function format()
    {
        return $this->content . "\n";
    }

    private function kv()
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
    private $elements = [];
    private $link = [];

    function __construct($elements, $link)
    {
        $this->elements = $elements;
        $this->link = $link;
    }

    function format()
    {
        $s = '';
        foreach ($this->elements as $node) {
            $s .= $node->format();
        }
        return $s;
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
    private $name;
    private $fields;

    function __construct(string $name, c_composite_type $fields)
    {
        $this->name = $name;
        $this->fields = $fields;
    }

    function format()
    {
        return 'struct ' . $this->name . ' ' . $this->fields->format() . ";\n";
    }
}

class c_compat_struct_forward_declaration
{
    private $name;

    function __construct($name)
    {
        $this->name = $name;
    }

    function format()
    {
        return 'struct ' . $this->name->format() . ";\n";
    }
}

class c_composite_type
{
    public $fieldlists = [];

    function format()
    {
        $s = '';
        foreach ($this->fieldlists as $fieldlist) {
            $s .= $fieldlist->format() . "\n";
        }
        return "{\n"
            . indent($s) . "}";
    }
}

class c_defer
{
    public $expression;

    function format()
    {
        return 'defer ' . $this->expression->format() . ';';
    }
}

class c_ellipsis
{
    function format()
    {
        return '...';
    }
}

class c_enum_member
{
    public $id;
    public $value;

    function format()
    {
        $s = $this->id->format();
        if ($this->value) {
            $s .= ' = ' . $this->value->format();
        }
        return $s;
    }
}
class c_compat_enum
{
    private $hidden;
    private $members;

    function __construct($members, $hidden)
    {
        $this->members = $members;
        $this->hidden = $hidden;
    }

    function is_private()
    {
        return $this->hidden;
    }

    function format()
    {
        $s = "enum {\n";
        foreach ($this->members as $i => $member) {
            if ($i > 0) {
                $s .= ",\n";
            }
            $s .= "\t" . $member->format();
        }
        $s .= "\n};\n";
        return $s;
    }
}
class c_enum
{
    public $members = [];
    public $pub;

    function format()
    {
        $s = '';
        if ($this->pub) {
            $s .= 'pub ';
        }
        $s .= "enum {\n";
        foreach ($this->members as $id) {
            $s .= "\t" . $id->format() . ",\n";
        }
        $s .= "}\n";
        return $s;
    }

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

    function format()
    {
        return $this->str;
    }
}

class c_for
{
    public $init;
    public $condition;
    public $action;
    public $body;

    function format()
    {
        return sprintf(
            'for (%s; %s; %s) %s',
            $this->init->format(),
            $this->condition->format(),
            $this->action->format(),
            $this->body->format()
        );
    }
}

class c_function_arguments
{
    public $arguments = [];

    function format()
    {
        $s = '(';
        foreach ($this->arguments as $i => $argument) {
            if ($i > 0) $s .= ', ';
            $s .= $argument->format();
        }
        $s .= ')';
        return $s;
    }
}

class c_function_call
{
    private $function;
    private $arguments;

    function __construct($function, $arguments)
    {
        $this->function = $function;
        $this->arguments = $arguments;
    }

    function format()
    {
        return $this->function->format() . $this->arguments->format();
    }
}

class c_function_declaration
{
    private $pub;
    private $type;
    private $form;
    private $parameters;
    private $body;

    function __construct($pub, $type, $form, $parameters, $body)
    {
        $this->pub = $pub;
        $this->type = $type;
        $this->form = $form;
        $this->parameters = $parameters;
        $this->body = $body;
    }

    function format()
    {
        $s = sprintf(
            "%s %s%s %s\n\n",
            $this->type->format(),
            $this->form->format(),
            $this->parameters->format(),
            $this->body->format()
        );
        if ($this->pub) {
            return 'pub ' . $s;
        }
        return $s;
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

    function format()
    {
        $s = $this->type->format() . ' ';
        foreach ($this->forms as $i => $form) {
            if ($i > 0) $s .= ', ';
            $s .= $form->format();
        }
        return $s;
    }

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

class c_identifier
{
    public $name;

    static function make(string $name)
    {
        $self = new self;
        $self->name = $name;
        return $self;
    }

    function format()
    {
        return $this->name;
    }
}

class c_if
{
    public $condition;
    public $body;
    public $else = null;

    function format()
    {
        $s = sprintf('if (%s) %s', $this->condition->format(), $this->body->format());
        if ($this->else) {
            $s .= ' else ' . $this->else->format();
        }
        return $s;
    }
}

class c_import
{
    public $path;

    function format()
    {
        return "import $this->path\n";
    }

    function name()
    {
        return $this->path;
    }
}

class c_literal
{
    public $value;
    public $type;

    function format()
    {
        switch ($this->type) {
            case 'string':
                return '"' . $this->value . '"';
            case 'char':
                return '\'' . $this->value . '\'';
            default:
                return $this->value;
        }
    }
}

class c_loop_counter_declaration
{
    public $type;
    public $name;
    public $value;

    function format()
    {
        $s = sprintf(
            '%s %s = %s',
            $this->type->format(),
            $this->name->format(),
            $this->value->format()
        );
        return $s;
    }
}

class c_module
{
    public $elements = [];

    function format()
    {
        $s = '';
        foreach ($this->elements as $node) {
            $s .= $node->format();
        }
        return $s;
    }

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
    private $type;
    private $form;
    private $value;

    function __construct($type, $form, $value)
    {
        $this->type = $type;
        $this->form = $form;
        $this->value = $value;
    }

    function format()
    {
        return $this->type->format()
            . ' ' . $this->form->format()
            . ' = ' . $this->value->format()
            . ";\n";
    }
}

class c_postfix_operator
{
    private $operand;
    private $operator;

    function __construct($operand, $operator)
    {
        $this->operand = $operand;
        $this->operator = $operator;
    }

    function format()
    {
        return $this->operand->format() . $this->operator;
    }
}

class c_prefix_operator
{
    private $operand;
    private $operator;

    function __construct($operator, $operand)
    {
        $this->operator = $operator;
        $this->operand = $operand;
    }

    function format()
    {
        if ($this->operand instanceof c_binary_op || $this->operand instanceof c_cast) {
            return $this->operator . '(' . $this->operand->format() . ')';
        }
        return $this->operator . $this->operand->format();
    }
}

class c_return
{
    public $expression;

    function format()
    {
        if (!$this->expression) {
            return 'return;';
        }
        return 'return ' . $this->expression->format() . ';';
    }
}

class c_sizeof
{
    public $argument;

    function format()
    {
        return 'sizeof(' . $this->argument->format() . ')';
    }
}

class c_struct_fieldlist
{
    public $type;
    public $forms = [];

    function format()
    {
        $s = $this->type->format() . ' ';
        foreach ($this->forms as $i => $form) {
            if ($i > 0) $s .= ', ';
            $s .= $form->format();
        }
        $s .= ';';
        return $s;
    }
}

class c_struct_literal_member
{
    public $name;
    public $value;

    function format()
    {
        return '.' . $this->name->format() . ' = ' . $this->value->format();
    }
}

class c_struct_literal
{
    public $members = [];

    function format()
    {
        $s = "{\n";
        foreach ($this->members as $member) {
            $s .= "\t" . $member->format() . ",\n";
        }
        $s .= "}\n";
        return $s;
    }
}

class c_switch_case
{
    public $value;
    public $statements = [];

    function format()
    {
        $s = 'case ' . $this->value->format() . ": {\n";
        foreach ($this->statements as $statement) {
            $s .= $statement->format() . ";\n";
        }
        $s .= "}\n";
        return $s;
    }
}

class c_switch_default
{
    public $statements = [];

    function format()
    {
        $s = "default: {\n";
        foreach ($this->statements as $statement) {
            $s .= $statement->format() . ";\n";
        }
        $s .= "}\n";
        return $s;
    }
}

class c_switch
{
    public $value;
    public $cases = [];
    public $default;

    function format()
    {
        $s = '';
        foreach ($this->cases as $case) {
            $s .= $case->format() . "\n";
        }
        if ($this->default) {
            $s .= $this->default->format() . "\n";
        }
        return sprintf(
            "switch (%s) {\n%s\n}",
            $this->value->format(),
            indent($s)
        );
    }
}

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

    function format()
    {
        $s = '';
        if ($this->const) {
            $s .= 'const ';
        }
        $s .= $this->type;
        return $s;
    }
}

class c_union_field
{
    public $type;
    public $form;

    function format()
    {
        return $this->type->format() . ' ' . $this->form->format() . ';';
    }
}

class c_union
{
    public $form;
    public $fields = [];

    function format()
    {
        $s = '';
        foreach ($this->fields as $field) {
            $s .= $field->format() . "\n";
        }
        return sprintf("union {\n%s\n} %s;\n", indent($s), $this->form->format());
    }
}

class c_variable_declaration
{
    public $type;
    public $forms = [];
    public $values = [];

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

class c_while
{
    public $condition;
    public $body;

    function format()
    {
        return sprintf('while (%s) %s', $this->condition->format(), $this->body->format());
    }
}
