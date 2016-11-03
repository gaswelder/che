<?php
/*
 * Functions to parse packages.
 */

/*
The problem with merging is that C can't be parsed without additional
information about existing types. In case of packages, a type defined
in one fragment may be used in another fragment. In order to merge
fragments, they have to be parsed first, but in order to parse them,
they have to be merged first.

To untangle this, we have to do preliminary "discovery" of types in
the package by looking at typedefs at the lexer's level.
*/

class packages
{
	/*
	 * Parses a package at the given path (which must be a directory).
	 * Returns a 'module' object like the regular 'parse_module'.
	 */
	static function parse($path)
	{
		/*
		 * 'types' is a map: filepath => list of typenames
		 */
		$types = self::typenames($path);

		$mods = array();

		/*
		 * If we just get all the type names together and use them
		 * to parse the files, the parser will fail because it will
		 * encounter typedefs for already known types (the ones we
		 * have just collected). So we have to parse each file giving
		 * the parser only type names defined only in the other files.
		 */
		$files = glob("$path/*.c");
		foreach ($files as $fpath) {
			/*
			 * Get typenames defined not in this file
			 */
			$context = array();
			foreach ($types as $k => $list) {
				if ($k == $fpath) continue;
				$context = array_merge($context, $list);
			}
			/*
			 * Parse
			 */
			$mods[] = parse_module($fpath, $context);
		}
		return self::merge($mods);
	}

	/*
	 * Merges given array of modules, returns one module.
	 */
	private static function merge($mods)
	{
		$mod = new module();

		$imports = array();

		foreach ($mods as $m) {
			$mod->deps = array_merge($mod->deps, $m->deps);
			$mod->link = array_merge($mod->link, $m->link);

			/*
			 * Merge the code removing redundant imports.
			 */
			foreach ($m->code as $c) {
				/*
				 * If this import has been here already, skip it.
				 */
				if ($c instanceof c_import) {
					if (in_array($c->path, $imports)) {
						continue;
					}
					$imports[] = $c->path;
				}
				$mod->code[] = $c;
			}
		}

		$mod->deps = array_unique($mod->deps);
		$mod->link = array_merge($mod->link);
		return $mod;
	}

	/*
	 * Scans all package files and returns names of types
	 * declared in them.
	 */
	private static function typenames($path)
	{
		$list = array();
		$files = glob("$path/*.c");
		foreach ($files as $fpath) {
			$list[$fpath] = self::typenames_f($fpath);
		}
		return $list;
	}

	/*
	 * Returns list of types declared in the given file.
	 */
	private static function typenames_f($path)
	{
		$list = array();
		/*
		 * Scan file tokens for 'typedef' keywords
		 */
		$s = new mctok($path);
		while (1) {
			$t = $s->get();
			if ($t === null) {
				break;
			}
			if ($t->type != 'typedef') {
				continue;
			}
			/*
			 * When a 'typedef' is encountered, look ahead
			 * to find the type name
			 */
			$name = self::get_typename($s, $path);
			if (!$name) break;
			$list[] = $name;
		}
		return $list;
	}

	private static function get_typename(mctok $s, $path)
	{
		/*
		 * New type name is at the end of the typedef statement.
		 * It may have a value form or a function form.
		 */

		/*
		 * Get all tokens until the semicolon.
		 */
		$buf = array();
		while (!$s->ended()) {
			$t = $s->get();
			if ($t->type == ';') {
				break;
			}
			$buf[] = $t;
		}

		if (empty($buf)) {
			trigger_error("No tokens after 'typedef'");
			return null;
		}

		$buf = array_reverse($buf);

		/*
		 * We assume that function typedefs end with "(...)".
		 * In that case we omit that part.
		 */
		if ($buf[0]->type == ')') {
			while (!empty($buf)) {
				$t = array_shift($buf);
				if ($t->type == '(') {
					break;
				}
			}
		}

		/*
		 * The last 'word' token is assumed to be the type name.
		 */
		$name = null;
		while (!empty($buf)) {
			$t = array_shift($buf);
			if ($t->type == 'word') {
				$name = $t->content;
				break;
			}
		}

		if (!$name) {
			trigger_error("Type name expected in the typedef");
			return null;
		}
		return $name;
	}
}

?>
