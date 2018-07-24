<?php

class c_struct_identifier extends c_element
{
	public $name;

	function format()
	{
		if ($this->name) {
			return 'struct ' . $this->name->format();
		}
		return 'struct';
	}

	static function parse(parser $parser)
	{
		list($id) = $parser->seq('struct', '$identifier');
		$sid = new c_struct_identifier();
		$sid->name = $id;
		return $sid;
	}
}
