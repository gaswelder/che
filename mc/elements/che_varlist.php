<?php

/*
 * Variable declarations list like "int a, *b, c()"
 */
class che_varlist
{
	public $stat = false;
	public $type;
	public $forms;
	public $values;

	function __construct(che_type $type)
	{
		$this->type = $type;
		$this->forms = array();
	}

	function typenames()
	{
		$names = $this->type->typenames();
		foreach ($this->values as $expr) {
			if ($expr == null) continue;
			$names = array_merge($names, $expr->typenames());
		}
		return $names;
	}

	function add($var)
	{
		if ($var instanceof che_form) {
			$this->forms[] = $var;
			$this->values[] = null;
		} else {
			// assignment
			$this->forms[] = $var[0];
			$this->values[] = $var[1];
		}
	}

	function format()
	{
		$s = $this->type->format();
		foreach ($this->forms as $i => $f) {
			if ($i > 0) {
				$s .= ', ';
			} else {
				$s .= ' ';
			}
			$s .= $f->format();
			if ($this->values[$i] !== null) {
				$s .= ' = ' . $this->values[$i]->format();
			}
		}

		if ($this->stat) {
			$s = 'static ' . $s;
		}
		return $s;
	}
}
