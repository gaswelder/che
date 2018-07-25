<?php

class che_assignment extends che_element
{
	public $left;
	public $right;

	static function parse(parser $parser)
	{
		list($left, $right) = $parser->seq('$che_form', '=', '$che_expr');
		$a = new self;
		$a->left = $left;
		$a->right = $right;
		return $a;
	}

	function format()
	{
		return $this->left->format() . ' = ' . $this->right->format;
	}
}
