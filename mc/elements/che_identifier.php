<?php

class che_identifier extends che_element
{
	private $content;

	function format()
	{
		return $this->content;
	}

	static function parse(parser $parser)
	{
		$name = $parser->expect('word')->content;
		if ($parser->is_typename($name)) {
			throw new ParseException("typename, not id");
		}
		$id = new self;
		$id->content = $name;
		return $id;
	}
}
