<?php

class c_literal extends c_element
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

class c_string extends c_literal
{
	function format()
	{
		return '"' . $this->content . '"';
	}
}

class c_array extends c_literal
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
		return new c_array($elements);
	}
}

class c_designated_array_element extends c_element
{
	public $index;
	public $value;

	function format()
	{
		return "[$this->index] = " . $this->value->format();
	}

	static function parse(parser $parser)
	{
		$item = new c_designated_array_element;
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

class che_number extends c_literal
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

class che_char extends c_literal
{
	function format()
	{
		return "'" . $this->content . "'";
	}
}

class c_comment extends c_literal
{
	function format()
	{
		return "/* $this->content */";
	}

	static function parse(parser $parser)
	{
		$t = $parser->expect('comment');
		return new c_comment($t->content);
	}
}