<?php

class che_for extends che_element
{
	public $init;
	public $cond;
	public $act;
	public $body;

	function __construct($i, $c, $a, $b)
	{
		$this->init = $i;
		$this->cond = $c;
		$this->act = $a;
		$this->body = $b;
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
		$init = $parser->any(['che_varlist', 'che_expr']);
		list($cond, $act, $body) = $parser->seq(';', '$che_expr', ';', '$che_expr', ')', '$che_body');
		return new self($init, $cond, $act, $body);
	}
}
