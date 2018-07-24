<?php

class che_for
{
	public $init;
	// c_expr
	public $cond;
	// c_expr;
	public $act;
	public $body;
	function __construct($i, $c, $a, $b)
	{
		$this->init = $i;
		$this->cond = $c;
		$this->act = $a;
		$this->body = $b;
	}

	function typenames()
	{
		return array_merge(
			$this->init->typenames(),
			$this->cond->typenames(),
			$this->act->typenames(),
			$this->body->typenames()
		);
	}

	function format($tab = 0)
	{
		$p = str_repeat("\t", $tab);
		$s = $p . sprintf(
			"for (%s; %s; %s) ",
			$this->init->format(),
			$this->cond->format(),
			$this->act->format()
		);
		$s .= $this->body->format($tab);
		return $s;
	}

	static function parse(parser $parser)
	{
		$parser->seq('for', '(');
		$init = $parser->any(['varlist', 'expr']);
		list($cond, $act, $body) = $parser->seq(';', '$expr', ';', '$expr', ')', '$body');
		return new self($init, $cond, $act, $body);
	}
}
