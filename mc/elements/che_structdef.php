<?php

class che_structdef extends che_element
{
	public $pub;
	public $name;
	public $fields = array();

	function typenames()
	{
		$names = [];
		foreach ($this->fields as $list) {
			$names = array_merge($names, $list->typenames());
		}
		return $names;
	}

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

	function add(che_varlist $list)
	{
		$this->fields[] = $list;
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
		$lists = $parser->many('struct-def-element');
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
