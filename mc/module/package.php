<?php

/*
The problem with merging is that C can't be parsed without additional
information about existing types. In case of packages, a type defined
in one fragment may be used in another fragment. In order to merge
fragments, they have to be parsed first, but in order to parse them,
they have to be merged first.

To untangle this, we have to do preliminary "discovery" of types in
the package by looking at typedefs at the lexer's level.
 */


class package
{
	public $mods = [];
	public $path;

	/*
	 * Parses a package at the given path (which must be a directory).
	 * Returns a 'module' object like.
	 */
	static function parse($path)
	{
		$pack = new package;
		$pack->path = $path;

		// Scan all package files and get type names:
		// filepath => list of typenames
		$types = array();
		$files = $pack->list_files();
		foreach ($files as $file) {
			$types[$file->path] = $file->typenames();
		}

		$mods = array();

		/*
		 * If we just get all the type names together and use them
		 * to parse the files, the parser will fail because it will
		 * encounter typedefs for already known types (the ones we
		 * have just collected). So we have to parse each file giving
		 * the parser only type names defined only in the other files.
		 */
		foreach ($files as $file) {
			$fpath = $file->path;
			/*
			 * Get typenames defined not in this file
			 */
			$context = array();
			foreach ($types as $k => $list) {
				if ($k == $fpath) continue;
				$context = array_merge($context, $list);
			}
			$mods[] = module::parse($fpath, $context);
		}

		$pack->mods = $mods;
		return $pack->merge();
	}

	// Merges the package files and returns the resulting module.
	private function merge()
	{
		$mod = new module();
		$mod->path = $this->path;

		$imports = array();

		foreach ($this->mods as $m) {
			$mod->deps = array_merge($mod->deps, $m->deps);
			$mod->link = array_merge($mod->link, $m->link);

			// Merge the code removing redundant imports.
			foreach ($m->code as $c) {
				// If this import has been here already, skip it.
				if ($c instanceof che_import) {
					if (in_array($c->path(), $imports)) {
						continue;
					}
					$imports[] = $c->path();
				}
				$mod->code[] = $c;
			}
		}

		$mod->deps = array_unique($mod->deps, SORT_REGULAR);
		$mod->link = array_merge($mod->link);
		return $mod;
	}

	private function list_files()
	{
		$files = glob("$this->path/*.c");
		/*
		 * Omit files specific to other systems
		 */
		$list = array();
		foreach ($files as $path) {
			/*
			 * Get second extension from the filename.
			 * For 'main.unix.c' this will return 'unix'.
			 * For 'main.c' this will return ''.
			 */
			$ext = pathinfo(pathinfo($path, PATHINFO_FILENAME), PATHINFO_EXTENSION);
			/*
			 * If the extension is present and doesn't match
			 * our platform, skip it.
			 */
			if ($ext && $ext != CHE_OS) {
				continue;
			}
			$list[] = $path;
		}

		$files = [];
		foreach ($list as $path) {
			$f = new package_file;
			$f->path = $path;
			$files[] = $f;
		}
		return $files;
	}
}
