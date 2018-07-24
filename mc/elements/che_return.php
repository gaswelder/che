<?php

class che_return extends che_expr
{
	function format($tab = 0)
	{
		return str_repeat("\t", $tab) . 'return ' . parent::format();
	}
}
