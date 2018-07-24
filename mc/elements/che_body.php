<?php

class c_body
{
	public $parts = array();

	function add($p)
	{
		$this->parts[] = $p;
	}

	function typenames()
	{
		$names = [];
		foreach ($this->parts as $part) {
			$cn = get_class($part);
			switch ($cn) {
				case 'che_varlist':
					$names = array_merge($names, $part->typenames());
					break;
				case 'c_if':
					$names = array_merge($names, $part->typenames());
					break;
				case 'che_while':
					$names = array_merge($names, $part->typenames());
					break;
				case 'che_for':
					$names = array_merge($names, $part->typenames());
					break;
				case 'che_switch':
					$names = array_merge($names, $part->typenames());
					break;
				case 'c_return':
				case 'c_expr':
					$names = array_merge($names, $part->typenames());
					break;
				default:
					var_dump("1706", $part);
					exit(1);
			}
		}
		return $names;
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
		$a = ['c_if', 'che_for', 'che_while', 'che_switch'];
		return in_array(get_class($part), $a);
	}

	static function parse(parser $parser)
	{
		$body = new c_body();

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
