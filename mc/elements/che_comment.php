<?php

class che_comment extends che_literal
{
	function format()
	{
		return "/* $this->content */";
	}

	static function parse(parser $parser)
	{
		$t = $parser->expect('comment');
		return new che_comment($t->content);
	}
}
