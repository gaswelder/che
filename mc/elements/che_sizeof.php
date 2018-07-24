<?php

class che_sizeof extends c_element
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

		$brace = false;
		if ($parser->maybe('(')) {
			$brace = true;
		}
		$arg = $parser->any(['typeform', 'expr']);
		if ($brace) {
			$parser->expect(')');
		}
		return new self($arg);
	}
}
