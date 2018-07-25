<?php

abstract class c_element
{
	function __toString()
	{
		$t = get_class($this);
		$s = json_encode($this, JSON_PRETTY_PRINT);
		return "$t$s";
	}

	function typenames()
	{
		return [];
	}

	abstract function format();
}

class c_macro extends c_element
{
	public $type;
	public $arg = null;
	function __construct($type, $arg = null)
	{
		$this->type = $type;
		$this->arg = $arg;
	}

	function format()
	{
		$s = '#' . $this->type;
		if ($this->arg !== null) {
			$s .= " $this->arg";
		}
		return $s;
	}
}

class c_define extends c_element
{
	public $name;
	public $value;

	function __construct($name, $value)
	{
		$this->name = $name;
		$this->value = $value;
	}

	function format()
	{
		return "#define $this->name $this->value\n";
	}
}

/*
 * A #link directive
 */
class c_link extends c_element
{
	public $name;
	function __construct($name)
	{
		$this->name = $name;
	}

	function format()
	{
		return "";
	}
}

class c_include extends c_element
{
	public $path;
	function __construct($path)
	{
		$this->path = $path;
	}

	function format()
	{
		return "#include $this->path\n";
	}
}

class c_prototype
{
	public $pub = false;
	public $type;
	public $form;
	public $args;

	function __construct(che_type $type, che_form $form, che_formal_args $args)
	{
		assert($form->name != "");
		$this->type = $type;
		$this->form = $form;
		$this->args = $args;
	}

	function typenames()
	{
		$names = $this->type->typenames();
		foreach ($this->args->groups as $group) {
			$names = array_merge($names, $group->type->typenames());
		}
		return $names;
	}

	function format()
	{
		$s = sprintf(
			"%s %s%s;",
			$this->type->format(),
			$this->form->format(),
			$this->args->format()
		);
		if (!$this->pub && $this->form->name != 'main') {
			$s = 'static ' . $s;
		}
		return $s;
	}
}

class c_forward_declaration extends c_element
{
	private $src;

	function __construct($src)
	{
		$this->src = $src;
	}

	function format()
	{
		return $this->src;
	}
}
