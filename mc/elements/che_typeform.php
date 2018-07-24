<?php

/*
 * A combination of type and form, like in "(struct foo *[])".
 */
class che_typeform
{
	public $type;
	public $form;
	function __construct($type, $form)
	{
		$this->type = $type;
		$this->form = $form;
	}

	function format()
	{
		return $this->type->format() . ' ' . $this->form->format();
	}

	static function parse(parser $parser)
	{
		$t = $parser->read('type');
		$f = $parser->read('form');
		return new self($t, $f);
	}
}
