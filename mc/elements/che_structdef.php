<?php

class che_structdef extends che_element
{
	public $pub;
	public $name;
	public $fields = array();

	function __construct($name = null, $lists = [])
	{
		if (!$name) $name = new che_struct_identifier();
		$this->name = $name;
		foreach ($lists as $list) {
			$this->add($list);
		}
	}

	function forward_declaration()
	{
		return new c_forward_declaration($this->name->format() . ';');
	}

	function add(che_struct_fields $fields)
	{
		$this->fields[] = $fields;
	}

	function format()
	{
		$s = $this->name->format();
		if (empty($this->fields)) {
			return $s;
		}

		$s .= " {\n";
		foreach ($this->fields as $list) {
			foreach ($list->forms as $f) {
				$s .= sprintf("\t%s %s;\n", $list->type->format(), $f->format());
			}
		}
		$s .= "}";
		return $s;
	}

	static function parse(parser $parser)
	{
		$pub = $parser->maybe('pub');
		list($id) = $parser->seq('$che_struct_identifier', '{');
		$lists = $parser->many('che_struct_fields');
		$parser->seq('}', ';');

		$def = new self($id, $lists);
		$def->pub = $pub;
		return $def;
	}

	function translate()
	{
		return [
			new c_struct_forward_declaration($this->name->format() . ";\n"),
			$this
		];
	}
}

class c_struct_forward_declaration
{
	private $s;
	function __construct($s)
	{
		$this->s = $s;
	}
	function format()
	{
		return $this->s;
	}
}

// One type, many forms.
class che_struct_fields extends che_element
{
	public $type;
	public $forms = [];

	static function parse(parser $parser)
	{
		$s = new self;
		$s->type = $parser->read('type');
		$s->forms[] = $parser->read('che_form');
		while ($parser->maybe(',')) {
			$s->forms[] = $parser->read('che_form');
		}
		$parser->expect(';');
		return $s;
	}

	function format()
	{
		$s = $this->type->format() . ' ';
		foreach ($this->forms as $i => $f) {
			if ($i > 0) $s .= ', ';
			$s .= $f->format();
		}
		return $s;
	}
}
