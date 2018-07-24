<?php

class c_structdef extends c_element
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
		if (!$name) $name = new c_struct_identifier();
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
		list($id) = $parser->seq('$struct-identifier', '{');
		$lists = $parser->many('struct-def-element');
		$parser->seq('}', ';');

		$def = new c_structdef($id, $lists);
		$def->pub = $pub;
		return $def;
	}
}
