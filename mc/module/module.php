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

	/**
	 * Reorders the given list of C elements so that the native top-down
	 * compiler would be able to work.
	 */
	static function stable_elements_sort($elements)
	{
		// Generally we split elements into groups by their kind
		// and then lay them out group by group. For example, all
		// headers come before all functions.
		//
		// But the original order of some elements inside each group
		// sometimes matters (for example, when one structure definition
		// embeds a structure defined earlier). That's why we use the
		// stable sort here.

		$tmp = [];
		foreach ($elements as $i => $e) {
			$tmp[] = [$e, $i];
		}

		$order = function ($obj) {
			$cn = get_class($obj);
			$ranks = [
				[c_define::class, c_include::class],
				[che_enum::class],
				[c_struct_forward_declaration::class],
				[che_typedef::class],
				[che_structdef::class],
				[c_prototype::class],
				[che_varlist::class]
			];
			foreach ($ranks as $i => $n) {
				if (in_array($cn, $n)) return $i;
			}
			return $i + 1;
		};

		usort($tmp, function ($a, $b) use ($order) {
			// If the elements have the same rank, return the same ordering
			// as they had originally.
			// Otherwise reorder as usual.
			$i = $order($a[0]);
			$j = $order($b[0]);
			if ($i == $j) {
				return $a[1] <=> $b[1];
			}
			return $i <=> $j;
		});
		return array_column($tmp, 0);
	}

	/**
	 * Returns a C module translated from this module.
	 */
	function translate() : c_module
	{
		$elements = [];
		foreach ($this->code as $element) {
			$elements = array_merge($elements, $element->translate());
		}

		$std = [
			'assert',
			'ctype',
			'errno',
			'limits',
			'math',
			'stdarg',
			'stdbool',
			'stddef',
			'stdint',
			'stdio',
			'stdlib',
			'string',
			'time'
		];
		foreach ($std as $n) {
			$elements[] = new c_include("<$n.h>");
		}

		// Filter out link pragmas and add them to the module.
		$links = array_filter($elements, function ($e) {
			return $e instanceof c_link;
		});
		$elements = array_filter($elements, function ($e) {
			return !($e instanceof c_link);
		});

		// Reorder the code.
		$elements = self::stable_elements_sort($elements);

		$c = new c_module;
		$c->code = $elements;
		$c->link = ['m'];
		foreach ($links as $link) {
			$c->link[] = $link->name;
		}
		$c->name = $this->path;
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

	// Returns synopsis that would be the contents of this module' s
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
