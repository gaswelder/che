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
	private $path;

	/**
	 * Returns a package from the given path.
	 */
	static function read($name, $refdir = '.') : self
	{
		$path = self::find($name, $refdir);
		if (!$path) {
			throw new Exception("couldn't find package '$name'");
		}
		$pack = new self;
		$pack->path = $path;
		return $pack;
	}

	private static function find($name, $refdir)
	{
		if ($name[0] == '.') {
			$name = substr($name, 1);
			$p = array($refdir . $name);
		} else {
			$p = array(
				MCDIR . "/lib/$name",
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

	/**
	 * Returns an array of parsed modules from this package.
	 */
	private function modules() : array
	{
		$types = $this->types();

		/*
		 * If we just get all the type names together and use them
		 * to parse the files, the parser will fail because it will
		 * encounter typedefs for already known types (the ones we
		 * have just collected). So we have to parse each file giving
		 * the parser only type names defined only in the other files.
		 */
		$get_types = function ($path) use ($types) {
			$list = [];
			foreach ($types as $k => $v) {
				if ($k == $path) continue;
				$list = array_merge($list, $v);
			}
			return $list;
		};

		$mods = [];
		foreach ($this->files() as $file) {
			$mods[] = module::parse($file->path(), $get_types($file->path()));
		}
		return $mods;
	}

	function types() : array
	{
		// Scan all package files and get type names:
		// filepath => list of typenames
		$types = array();
		foreach ($this->files() as $file) {
			$types[$file->path()] = $file->typenames();
		}
		return $types;
	}

	/**
	 * Merges the package files and returns the resulting module.
	 */
	function merge() : module
	{
		$mods = $this->modules();
		while (count($mods) > 1) {
			$a = array_shift($mods);
			$b = array_shift($mods);
			array_unshift($mods, $a->merge($b));
		}
		$mods[0]->path = $this->path;
		return $mods[0];
	}

	/**
	 * Returns list of files that this package consists of.
	 */
	private function files() : array
	{
		$paths = is_dir($this->path) ? glob("$this->path/*.c") : [$this->path];

		$files = array_map(function (string $path) : package_file {
			return new package_file($path);
		}, $paths);

		// Exclude files specific to other systems.
		$files = array_filter($files, function (package_file $f) {
			return $f->suits_system(CHE_OS);
		});

		return $files;
	}
}
