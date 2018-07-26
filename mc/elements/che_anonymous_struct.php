<?php

class che_anonymous_struct extends che_structdef
{
	static function parse(parser $parser)
	{
		$parser->seq('struct', '{');
		$lists = $parser->many('struct-def-element');
		$parser->seq('}');

		$def = new self;
		foreach ($lists as $list) {
			$def->add($list);
		}
		return $def;
	}
}
