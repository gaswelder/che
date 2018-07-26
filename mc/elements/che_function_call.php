<?php

class che_function_call extends che_element
{
	public $args = [];

	static function parse(parser $parser)
	{
		$call = new self();

		$parser->expect('(');
		while (1) {
			try {
				$call->args[] = $parser->read('che_expr');
			} catch (ParseException $e) {
				break;
			}
			try {
				$parser->expect(',');
			} catch (ParseException $e) {
				break;
			}
		}
		$parser->expect(')');
		return $call;
	}

	function format()
	{
		$list = [];
		foreach ($this->args as $arg) {
			$list[] = $arg->format();
		}
		return '(' . implode(', ', $list) . ')';
	}
}
