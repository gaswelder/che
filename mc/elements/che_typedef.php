<?php

class che_typedef extends che_element
{
	public $form;
	public $type;

	function __construct(che_type $type, che_form $form)
	{
		if (!$form->name) {
			trigger_error("typedef: empty type name");
		}
		$this->type = $type;
		$this->form = $form;
	}

	function format()
	{
		return sprintf("typedef %s %s", $this->type->format(), $this->form->format());
	}

	static function parse(parser $parser)
	{
		list($type, $form) = $parser->seq('typedef', '$type', '$form', ';');
		$parser->add_type($form->name);
		return new self($type, $form);
	}
}
