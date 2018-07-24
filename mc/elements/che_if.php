<?php

class c_if
{
	public $cond;
	// c_expr
	public $body;
	// c_body
	public $else;

	function typenames()
	{
		$b = $this->body->typenames();
		$names = array_merge($this->cond->typenames(), $b);
		if ($this->else) {
			$names = array_merge($names, $this->else->typenames());
		}
		return $names;
	}

	function __construct($cond, $body, $else)
	{
		$this->cond = $cond;
		$this->body = $body;
		$this->else = $else;
	}

	function format($tab = 0)
	{
		$pref = str_repeat("\t", $tab);
		$s = $pref . sprintf("if (%s) ", $this->cond->format());
		$s .= $this->body->format($tab);
		if ($this->else) {
			$s .= $pref . "else " . $this->else->format($tab);
		}
		return $s;
	}
}
