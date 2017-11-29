<?php
class module
{
	public $code = array();
	public $deps = array();
	public $link = array();

	function format_as_c()
	{
		return format_che($this->code);
	}
}

class import
{
	public $path;
	public $code = array();
}

class modules
{
	static function parse($path, $typenames = array())
	{
		$mod = new module();

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
				$imp = get_import($t->path, $t->dir);
				foreach ($imp->code as $decl) {
					if ($decl instanceof c_typedef) {
						$s->add_type($decl->form->name);
					}
				}
				/*
				 * Add the module's path to the dependencies list
				 */
				$mod->deps[] = $imp->path;
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

/*
 * Parses a module at given path
 * and returns a 'module' object
 */
function parse_module($path, $typenames = array())
{
	static $mods = array();

	if (isset($mods[$path])) {
		return $mods[$path];
	}

	if (is_dir($path)) {
		$mod = packages::parse($path);
	}
	else {
		$mod = modules::parse($path, $typenames);
	}

	$mods[$path] = $mod;
	return $mod;
}

/*
 * Finds the given module, parses it
 * and returns an 'import' object.
 */
function get_import($modname, $refdir)
{
	$path = find_import($modname, $refdir);
	if (!$path) {
		fwrite(STDERR, "Could not find module: $modname\n");
		if ($modname[0] != '.' && MCDIR == '.') {
			fwrite(STDERR, "CHE_HOME environment variable is not defined\n");
		}
		exit(1);
	}

	$mod = parse_module($path);

	$imp = new import();
	$imp->path = $path;

	foreach ($mod->code as $element) {
		$cn = get_class($element);
		switch ($cn) {
		case 'c_typedef':
			$imp->code[] = $element;
			break;
		case 'c_structdef':
		case 'c_enum':
			if ($element->pub){
				$imp->code[] = $element;
			}
			break;
		case 'c_func':
			$dec = $element->proto;
			if ($dec->pub){
				$imp->code[] = $dec;
			}
			break;
		}
	}
	return $imp;
}

/*
 * Returns path to the specified module.
 */
function find_import($name, $refdir)
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

?>
