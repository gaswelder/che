<?php

class che_struct_identifier extends che_element
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
		list($id) = $parser->seq('struct', '$che_identifier');
		$sid = new self();
		$sid->name = $id;
		return $sid;
	}
}
