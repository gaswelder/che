<?php

/*
 * Formal arguments list for a function prototype, like '(int a, int *b)'.
 */
class c_formal_args
{
	public $groups = [];
	public $more = false; // ellipsis

	function add(c_formal_argsgroup $g)
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

class c_formal_argsgroup extends c_element
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
