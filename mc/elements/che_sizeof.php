<?php

class c_sizeof extends c_element
{
	private $arg;

	function __construct($arg)
	{
		$this->arg = $arg;
	}

	function format()
	{
		$s = 'sizeof (';
		$s .= $this->arg->format();
		$s .= ')';
		return $s;
	}

	static function parse(parser $parser)
	{
		$parser->expect('sizeof');
		try {
			list($arg) = $parser->seq('(', '$sizeof-contents', ')');
		} catch (ParseException $e) {
			list($arg) = $parser->seq('$sizeof-contents');
		}
		return new c_sizeof($arg);
	}
}
