<?php

class che_while
{
	public $cond;
	public $body;

	function __construct($cond, $body)
	{
		$this->cond = $cond;
		$this->body = $body;
	}

	function format($tab = 0)
	{
		$p = str_repeat("\t", $tab);
		$s = $p . sprintf("while (%s) ", $this->cond->format());
		$s .= $this->body->format($tab);
		return $s;
	}

	static function parse(parser $parser)
	{
		list($cond, $body) = $parser->seq('while', '(', '$expr', ')', '$body');
		return new self($cond, $body);
	}
}
