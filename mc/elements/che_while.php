<?php

class che_while extends che_element
{
	public $cond;
	public $body;

	function __construct($cond, $body)
	{
		$this->cond = $cond;
		$this->body = $body;
	}

	function format()
	{
		$s = sprintf("while (%s) ", $this->cond->format());
		$s .= $this->body->format();
		return $s;
	}

	static function parse(parser $parser)
	{
		list($cond, $body) = $parser->seq('while', '(', '$che_expr', ')', '$che_body');
		return new self($cond, $body);
	}
}
