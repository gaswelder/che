<?php

class module
{
	public $code = array();
	public $link = array();

	// List of modules imported by this module.
	public $deps = [];

	// Path to this module's file.
	public $path;

	function format_as_c()
	{
		return format_che($this->code);
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

	// Returns the list of this module's dependencies.
	function dependencies()
	{

	}

	static function import($modname, $refdir)
	{
		$path = find_import($modname, $refdir);
		if (!$path) {
			fwrite(STDERR, "Could not find module: $modname\n");
			if ($modname[0] != '.' && MCDIR == '.') {
				fwrite(STDERR, "CHE_HOME environment variable is not defined\n");
			}
			exit(1);
		}
		return self::parse($path);
	}

	static function parse($path, $typenames = array())
	{
		static $mods = array();

		if (isset($mods[$path])) {
			return $mods[$path];
		}
	
		if (is_dir($path)) {
			$mod = packages::parse($path);
			$mods[$path] = $mod;
			return $mod;
		}

		$mod = new module();
		$mod->path = $path;

		$s = new parser($path);
		foreach ($typenames as $name) {
			$s->add_type($name);
		}

		while (!$s->ended()) {
			$t = $s->get();
			if ($t instanceof c_import) {
				/*
				 * Update the parser's types list
				 * from the referenced module
				 */
				$imp = module::import($t->path, $t->dir);
				foreach ($imp->synopsis() as $decl) {
					if ($decl instanceof c_typedef) {
						$s->add_type($decl->form->name);
					}
				}
				$mod->deps[] = $imp;
			}
			if ($t instanceof c_link) {
				$mod->link[] = $t->name;
			}

			$mod->code[] = $t;
		}

		$mods[$path] = $mod;
		return $mod;
	}
}
