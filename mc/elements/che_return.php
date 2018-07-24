<?php

class c_return extends c_expr
{
	function format($tab = 0)
	{
		return str_repeat("\t", $tab) . 'return ' . parent::format();
	}
}
