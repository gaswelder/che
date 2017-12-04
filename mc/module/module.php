<?php

class module
{
	public $code = array();
	public $link = array();

	// List of modules imported by this module.
	public $deps = [];

	// Path to this module's file.
	public $path;

	function info() {
		$deps = array_map(function($mod) {
			return $mod->info();
		}, $this->deps);
		return [
			'path' => $this->path,
			'deps' => $deps
		];
	}

	function translate_to_c()
	{
		$mod = clone $this;
		mc_trans::translate($mod);
		$c = new c_module;
		$c->code = $mod->code;
		$c->link = $mod->link;
		$c->name = $mod->path;
		return $c;
	}

	function format()
	{
		$s = '';
		foreach ($this->code as $e) {
			$s .= $e->format() . "\n";
		}
		return $s;
	}

	// Returns synopsis that would be the contents of this module's
	// C header file.
	function synopsis()
	{
		$code = [];
		foreach ($this->code as $element) {
			$cn = get_class($element);
			switch ($cn) {
			case 'c_typedef':
				$code[] = $element;
				break;
			case 'c_structdef':
			case 'c_enum':
				if ($element->pub){
					$code[] = $element;
				}
				break;
			case 'c_func':
				$dec = $element->proto;
				if ($dec->pub){
					$code[] = $dec;
				}
				break;
			}
		}
		return $code;
	}

	static function import($modname, $refdir)
	{
		$path = self::find_import($modname, $refdir);
		if (!$path) {
			fwrite(STDERR, "Could not find module: $modname\n");
			if ($modname[0] != '.' && MCDIR == '.') {
				fwrite(STDERR, "CHE_HOME environment variable is not defined\n");
			}
			exit(1);
		}
		return self::parse($path);
	}

	private static function find_import($name, $refdir)
	{
		if ($name[0] == '.') {
			$name = substr($name, 1);
			$p = array($refdir.$name);
		}
		else {
			$p = array(
				MCDIR."/lib/$name",
				"$refdir/$name",
				$name
			);
		}
		foreach ($p as $path) {
			if (file_exists($path)) {
				return $path;
			}
			$path .= ".c";
			if (file_exists($path)) {
				return $path;
			}
		}
		return null;
	}

	static function parse($path, $typenames = array())
	{
		static $mods = array();

		if (isset($mods[$path])) {
			return $mods[$path];
		}
	
		if (is_dir($path)) {
			$mod = package::parse($path);
			$mods[$path] = $mod;
			return $mod;
		}

		$s = new parser($path);
		foreach ($typenames as $name) {
			$s->add_type($name);
		}
		$mod = $s->parse();

		$mods[$path] = $mod;
		return $mod;
	}
}
