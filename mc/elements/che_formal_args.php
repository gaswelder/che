<?php

/*
 * Formal arguments list for a function prototype, like '(int a, int *b)'.
 */
class che_formal_args
{
	public $groups = [];
	public $more = false; // ellipsis

	function add(che_formal_argsgroup $g)
	{
		$this->groups[] = $g;
	}

	function format()
	{
		$list = [];
		foreach ($this->groups as $g) {
			$list[] = $g->format();
		}
		if ($this->more) {
			$list[] = '...';
		}
		return '(' . implode(', ', $list) . ')';
	}
}

class che_formal_argsgroup extends che_element
{
	public $type;
	public $forms = [];

	function format()
	{
		$list = [];
		foreach ($this->forms as $form) {
			$list[] = $this->type->format() . ' ' . $form->format();
		}
		return implode(', ', $list);
	}
}
