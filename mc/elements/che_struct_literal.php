<?php

class che_struct_literal extends c_element
{
	private $elements = [];

	function add($element)
	{
		$this->elements[] = $element;
	}

	function format()
	{
		$s = "{\n";
		foreach ($this->elements as $e) {
			$s .= "\t" . $e->format() . ",\n";
		}
		$s .= "}\n";
		return $s;
	}

	static function parse(parser $parser)
	{
		$struct = new self;
		$parser->expect('{');
		while (1) {
			try {
				$struct->add($parser->read('struct-literal-element'));
			} catch (ParseException $e) {
				break;
			}
			try {
				$parser->expect(',');
			} catch (ParseException $e) {
				break;
			}
		}
		$parser->expect('}');
		return $struct;
	}
}

class che_struct_literal_element extends c_element
{
	private $id;
	private $value;

	function __construct($id, $value)
	{
		$this->id = $id;
		$this->value = $value;
	}

	function format()
	{
		return sprintf('.%s = %s', $this->id->format(), $this->value->format());
	}
}
