<?php

class che_embedded_union extends che_element
{
	private $union;
	private $form;

	static function parse(parser $parser)
	{
		$s = new self;
		$s->union = $parser->read('union');
		$s->form = $parser->read('che_form');
		return $s;
	}

	function format()
	{
		return $this->union->format() . ' ' . $this->form->format();
	}
}
