<?php

/*
 * Type, a list of type modifiers like 'int' or 'struct {...}'.
 */
class che_type
{
	public $l;
	function __construct($list)
	{
		$this->l = $list;
	}

	function format()
	{
		$s = array();
		foreach ($this->l as $t) {
			if (!is_string($t)) $t = $t->format();
			$s[] = $t;
		}
		return implode(' ', $s);
	}
}
