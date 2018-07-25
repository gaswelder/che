<?php

class che_enum extends che_element
{
	public $pub = false;
	public $items = [];

	function format()
	{
		$lines = ['enum {'];
		foreach ($this->items as $item) {
			$lines[] = "\t" . $item->format() . ',';
		}
		$lines[] = '};';
		return implode("\n", $lines) . "\n";
	}

	static function parse(parser $parser)
	{
		$pub = $parser->maybe('pub');
		$parser->seq('enum', '{');

		$enum = new self();
		$enum->pub = $pub;
		while (1) {
			try {
				$enum->items[] = $parser->read('enum-item');
			} catch (ParseException $e) {
				break;
			}
			try {
				$parser->seq(',');
			} catch (ParseException $e) {
				break;
			}
		}
		$parser->seq('}', ';');
		return $enum;
	}
}

class che_enum_item extends che_element
{
	public $id;
	public $value;

	function format()
	{
		if ($this->value === null) {
			return $this->id->format();
		}
		return $this->id->format() . ' = ' . $this->value->format();
	}

	static function parse(parser $parser)
	{
		$item = new self();
		$item->id = $parser->read('identifier');

		if ($parser->maybe('=')) {
			$item->value = $parser->read('literal');
		}
		return $item;
	}
}
