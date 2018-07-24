<?php

class che_literal extends c_element
{
	public $content;
	function __construct($s)
	{
		$this->content = $s;
	}

	function format()
	{
		return $this->content;
	}
}

class che_string extends che_literal
{
	function format()
	{
		return '"' . $this->content . '"';
	}
}

class che_array extends che_literal
{
	function format()
	{
		$i = 0;
		$s = '{';
		foreach ($this->content as $e) {
			if ($i > 0) {
				$s .= ', ';
			}
			$i++;

			$s .= $e->format();
		}
		$s .= '}';
		return $s;
	}

	static function parse(parser $parser)
	{
		$parser->expect('{');
		$elements = [];
		while (1) {
			try {
				$elements[] = $parser->read('array-literal-element');
			} catch (ParseException $e) {
				break;
			}
			try {
				$parser->expect(',');
			} catch (ParseException $e) {
				break;
			}
		}
		$parser->expect('}');
		return new che_array($elements);
	}
}

class che_designated_array_element extends c_element
{
	public $index;
	public $value;

	function format()
	{
		return "[$this->index] = " . $this->value->format();
	}

	static function parse(parser $parser)
	{
		$item = new self;
		$parser->expect('[');
		try {
			$item->index = $parser->expect('word')->content;
		} catch (ParseException $e) {
			$item->index = $parser->expect('number')->content;
		}
		$parser->expect(']');
		$parser->expect('=');
		$item->value = $parser->read('constant-expression');
		return $item;
	}
}

class che_number extends che_literal
{
	function format()
	{
		return $this->content;
	}

	static function parse(parser $parser)
	{
		// Optional minus sign.
		$s = '';
		try {
			$parser->expect('-');
			$s = '-';
		} catch (ParseException $e) {
			//
		}
		// The number token.
		$s .= $parser->expect('num')->content;
		return new self($s);
	}
}

class che_char extends che_literal
{
	function format()
	{
		return "'" . $this->content . "'";
	}
}

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