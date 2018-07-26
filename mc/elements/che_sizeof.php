<?php

class che_sizeof extends che_element
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
		$arg = $parser->any(['che_typeform', 'che_expr']);
		if ($brace) {
			$parser->expect(')');
		}
		return new self($arg);
	}
}
