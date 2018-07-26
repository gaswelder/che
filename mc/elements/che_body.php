<?php

class che_body
{
	public $parts = array();

	function add($p)
	{
		$this->parts[] = $p;
	}

	function format()
	{
		$s = "{\n";
		foreach ($this->parts as $part) {
			$s .= tab($part->format());
			if (!self::is_construct($part)) {
				$s .= ';';
			}
			$s .= "\n";
		}
		$s .= "}\n";
		return $s;
	}

	static function is_construct($part)
	{
		$a = ['che_if', 'che_for', 'che_while', 'che_switch'];
		return in_array(get_class($part), $a);
	}

	static function parse(parser $parser)
	{
		$body = new self();

		$parser->expect('{');
		while (1) {
			try {
				$part = $parser->any([
					'che_comment',
					'if',
					'for',
					'while',
					'switch',
					'return',
					'body-varlist',
					'defer',
					'body-expr'
				]);
				$body->add($part);
			} catch (ParseException $e) {
				break;
			}
		}
		$parser->expect('}');
		return $body;
	}
}
