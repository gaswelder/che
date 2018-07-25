<?php

class module
{
	public $code = array();
	public $link = array();

	// List of modules imported by this module.
	public $deps = [];

	// Path to this module's file.
	public $path;

	function info()
	{
		$deps = array_map(function ($mod) {
			return $mod->info();
		}, $this->deps);
		return [
			'path' => $this->path,
			'deps' => $deps
		];
	}

	// function translate()
	// {
	// 	$elements = [];
	// 	foreach ($this->code as $element) {
	// 		$elements = array_merge($elements, $element->translate());
	// 	}
	// 	return c_module::make($elements);
	// }

	function translate_to_c()
	{
		$mod = clone $this;
		$code = $mod->code;

		$headers = array();
		$struct_forwards = [];
		$prototypes = array();
		$types = array();
		$body = array();

		$n = count($code);
		for ($i = 0; $i < $n; $i++) {
			$element = $code[$i];

			if ($element instanceof che_structdef) {
				$struct_forwards[] = $element->forward_declaration();
			}

			if ($element instanceof c_include || $element instanceof c_define) {
				$headers[] = $element;
				continue;
			}

			if ($element instanceof che_structdef || $element instanceof che_enum
				|| $element instanceof che_typedef) {
				$types[] = $element;
				continue;
			}

			if ($element instanceof che_import) {
				$imp = $element->get_module();
				array_splice($code, $i, 1, $imp->merge()->synopsis());
				$i--;
				$n = count($code);
				continue;
			}

			if ($element instanceof che_func) {
				$element = $element->translate();
				$prototypes[] = $element->proto;
			}

			/*
			 * Mark module-global variables as static.
			 */
			if ($element instanceof che_varlist) {
				$element->stat = true;
			}

			$body[] = $element;
		}

		$out = array_merge($headers, $struct_forwards, $types, $prototypes, $body);
		$mod->code = $out;

		$pos = 0;
		foreach ($mod->code as $e) {
			if ($e instanceof c_define || $e instanceof c_include) {
				$pos++;
			} else {
				break;
			}
		}

		$headers = tr_headers::add_headers($mod);
		$out = array();
		foreach ($headers as $h) {
			$out[] = new c_macro('include', "<$h.h>");
		}
		array_splice($mod->code, $pos, 0, $out);

		if (in_array('math', $headers)) {
			$mod->link[] = 'm';
		}

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
				case 'che_typedef':
					$code[] = $element;
					break;
				case 'che_structdef':
				case 'che_enum':
					if ($element->pub) {
						$code[] = $element;
					}
					break;
				case 'che_func':
					$dec = $element->proto;
					if ($dec->pub) {
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

	function merge(module $that) : module
	{
		$mod = new module;
		$mod->path = '';
		$mod->deps = array_merge($this->deps, $that->deps);
		$mod->link = array_merge($this->link, $that->link);
		$mod->code = array_merge($this->code, $that->code);

		$mod->deps = array_unique($mod->deps, SORT_REGULAR);

		// Remove redundant imports
		$imports = [];
		$mod->code = array_filter($mod->code, function ($c) use (&$imports) {
			if ($c instanceof che_import) {
				if (in_array($c->path(), $imports)) {
					return false;
				}
				$imports[] = $c->path();
			}
			return true;
		});

		return $mod;
	}

	static function parse($path, $typenames = array())
	{
		static $mods = array();

		if (isset($mods[$path])) {
			return $mods[$path];
		}

		if (is_dir($path)) {
			throw new Exception("Not a file: $path");
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
