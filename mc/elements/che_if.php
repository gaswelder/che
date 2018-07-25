<?php

class che_if
{
	public $cond;
	public $body;
	public $else;

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
