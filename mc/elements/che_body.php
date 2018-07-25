<?php

class che_body
{
	public $parts = array();

	function add($p)
	{
		$this->parts[] = $p;
	}

	function format($tab = 0)
	{
		$pref = str_repeat("\t", $tab);
		$s = "{\n";
		foreach ($this->parts as $part) {
			$s .= $part->format($tab + 1);
			if (!self::is_construct($part)) {
				$s .= ';';
			}
			$s .= "\n";
		}
		$s .= "$pref}\n";
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
					'comment',
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
