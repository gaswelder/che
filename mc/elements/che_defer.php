<?php

class che_defer
{
	public $expr;

	function __construct(che_expr $e)
	{
		$this->expr = $e;
	}

	function format()
	{
		return "defer " . $this->expr->format();
	}

	static function parse(parser $parser)
	{
		list($expr) = $parser->seq('defer', '$expr', ';');
		return new self($expr);
	}
}
