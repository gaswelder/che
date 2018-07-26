<?php

/*
 * Variable declarations list like "int a, *b, c()"
 */
class che_varlist extends che_element
{
	public $stat = false;
	public $type;
	public $forms;
	public $values;

	function __construct(che_type $type)
	{
		$this->type = $type;
		$this->forms = array();
	}

	static function parse(parser $parser)
	{
		$type = $parser->read('type');
		$list = new self($type);

		$next = $parser->any(['che_assignment', 'che_form']);
		$list->add($next);

		while (1) {
			try {
				$parser->expect(',');
			} catch (ParseException $e) {
				break;
			}

			try {
				$next = $parser->any(['che_assignment', 'che_form']);
				$list->add($next);
			} catch (ParseException $e) {
				break;
			}
		}
		return $list;
	}

	function add($var)
	{
		if ($var instanceof che_form) {
			$this->forms[] = $var;
			$this->values[] = null;
		} else if ($var instanceof che_assignment) {
			$this->forms[] = $var->left;
			$this->values[] = $var->right;
		} else {
			$this->forms[] = $var[0];
			$this->values[] = $var[1];

			// var_dump($var);
			// throw new Exception("Unknown type of variable");
		}
	}

	function format()
	{
		$s = $this->type->format();
		foreach ($this->forms as $i => $f) {
			if ($i > 0) {
				$s .= ', ';
			} else {
				$s .= ' ';
			}
			$s .= $f->format();
			if ($this->values[$i] !== null) {
				$s .= ' = ' . $this->values[$i]->format();
			}
		}

		if ($this->stat) {
			$s = 'static ' . $s;
		}
		return $s;
	}

	function translate()
	{
		return [$this];
	}
}
