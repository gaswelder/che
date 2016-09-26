<?php
define( 'MCDIR', '.' );
require 'mc/mc.php';

exit(main($argv));

function main( $args )
{
	array_shift( $args );
	$path = array_shift( $args );

	compile($path);
}

function compile($main)
{
	/*
	 * The modules list starts with the main source file.
	 * Parse the modules, adding newly discovered
	 * dependencies to the end of the list.
	 */
	$list = array($main);
	$n = count($list);
	for($i = 0; $i < $n; $i++)
	{
		$mod = parse_module($list[$i], getcwd());
		foreach($mod->deps as $path) {
			$list[] = $path;
			$n++;
		}
	}

	/*
	 * After all the dependencies have been parsed, the list may contain
	 * duplicate names due to common dependencies. The duplicates have
	 * to be removed leaving ones closer to the end of the list, so that
	 * [main, a, b, a, c] would become [main, b, a, c].
	 */
	$rev = array_reverse($list);
	$list = array();
	foreach($rev as $name) {
		if(!in_array($name, $list)) {
			$list[] = $name;
		}
	}
	$list = array_reverse($list);

	/*
	 * Translate the modules to C
	 */
	$sources = array();
	foreach($list as $path) {
		$mod = parse_module($path);
		$code = mc_trans::translate($mod->code);
		$tmppath = tmppath($path);
		file_put_contents($tmppath, mc::format($code));
		$sources[] = $tmppath;
	}

	/*
	 * Derive executable name from the main module
	 */
	$outname = basename($list[0]);
	if( $p = strrpos( $outname, '.' ) ) {
		$outname = substr( $outname, 0, $p );
	}

	/*
	 * Run the compiler
	 */
	exit(c99($sources, $outname));
}

function tmppath($path)
{
	$dir = 'mcbuild';
	if( !file_exists($dir) && !mkdir($dir) ) {
		exit(1);
	}
	return $dir.'/'.basename($path).'-'.md5($path).'.c';
}

function c99($sources, $name)
{
	$cmd = 'c99 -Wall -Wextra -Werror -pedantic -pedantic-errors';
	$cmd .= ' -fmax-errors=3 -D _XOPEN_SOURCE=700';
	$cmd .= ' -g ' . implode(' ', $sources);
	$cmd .= ' -o '.$name;
	exec($cmd, $output, $ret);
	return $ret;
}

class module {
	public $code = array();
	public $deps = array();
}

class import {
	public $path;
	public $code = array();
}

/*
 * Parses a module at given path
 * and returns a 'module' object
 */
function parse_module($path)
{
	static $mods = array();

	if(isset($mods[$path])) {
		return $mods[$path];
	}

	$mod = new module();

	$s = new parser($path);
	while(!$s->ended())
	{
		$t = $s->get();
		if($t instanceof c_import)
		{
			/*
			 * Update the parser's types list
			 * from the references module
			 */
			$imp = get_import($t->path, $t->dir);
			foreach($imp->code as $decl) {
				if($decl instanceof c_typedef) {
					$s->add_type($decl->name);
				}
			}
			/*
			 * Add the module's path to the dependencies list
			 */
			$mod->deps[] = $imp->path;
		}

		$mod->code[] = $t;
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
	if(!$path) {
		fwrite( STDERR, "Could not find module: $modname\n" );
		exit(1);
	}

	$mod = parse_module($path);

	$imp = new import();
	$imp->path = $path;


	foreach($mod->code as $element)
	{
		$cn = get_class($element);

		switch( $cn ) {
			case 'c_typedef':
			case 'c_structdef':
			case 'c_enum':
				$imp->code[] = $element;
				break;
			case 'c_func':
				$dec = $element->proto;
				if( !in_array( 'static', $dec->type ) ) {
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
	if($name[0] == '.') {
		$name = substr($name, 1);
		$p = array(
			$refdir.$name.".c"
		);
	}
	else {
		$p = array(
			MCDIR . "/lib/$name.c",
			MCDIR . "/lib/$name/main.c",
			$name . ".c",
			"$name/main.c"
		);
	}
	foreach( $p as $path ) {
		if( file_exists( $path ) ) {
			return $path;
		}
	}
	return null;
}

?>
