<?php

class c_typedef extends c_element
{
	public $form;
	public $type;

	function __construct(c_type $type, c_form $form)
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

	function typenames()
	{
		return $this->type->typenames();
	}

	static function parse(parser $parser)
	{
		list($type, $form) = $parser->seq('typedef', '$type', '$form', ';');
		$parser->add_type($form->name);
		return new c_typedef($type, $form);
	}
}
