<?php

class che_union
{
	public $fields = array();

	function add(che_varlist $list)
	{
		$this->fields[] = $list;
	}

	function format()
	{
		$s = "union {\n";
		foreach ($this->fields as $list) {
			$s .= "\t" . $list->format() . ";\n";
		}
		$s .= "}\n";
		return $s;
	}
}
