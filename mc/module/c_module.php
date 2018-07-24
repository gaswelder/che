<?php

class c_module
{
	public $link = [];
	public $code;
	public $name;

	function write()
	{
		$s = $this->format();
		$tmppath = $this->tmppath($this->name);
		file_put_contents($tmppath, $s);
		return $tmppath;
	}

	private function format()
	{
		$term = array(
			'c_typedef',
			'c_structdef',
			'che_varlist'
		);
		$out = '';
		foreach ($this->code as $element) {
			$out .= $element->format();
			$cn = get_class($element);
			if (in_array($cn, $term)) {
				$out .= ';';
			}
			$out .= "\n";
		}
		return $out;
	}

	private function tmppath()
	{
		$dir = sys_get_temp_dir() . '/chetmp';
		if (!file_exists($dir) && !mkdir($dir)) {
			exit(1);
		}
		return $dir . '/' . basename($this->name) . '-' . md5($this->name) . '.c';
	}
}
