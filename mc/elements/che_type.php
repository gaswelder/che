<?php

/*
 * Type, a list of type modifiers like 'int' or 'struct {...}'.
 */
class c_type
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

	function typenames()
	{
		$names = [];
		foreach ($this->l as $t) {
			if (is_string($t)) {
				$names[] = $t;
			} else {
				$names[] = $t->format();
			}
		}
		return $names;
	}
}
