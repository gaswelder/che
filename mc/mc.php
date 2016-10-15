<?php

require '/home/gas/code/php/lib/debug.php';
stop_on_error();
error_reporting( -1 );

$dir = dirname( __FILE__ );

require $dir . '/buf.php';
require $dir . '/tokens.php';
require $dir . '/parser.php';
require $dir . '/objects.php';
require $dir . '/translator.php';

function trace( $m, $s = null ) {
	fwrite( STDERR, "--- $m		$s\n" );
}

function put( $s ) {
	fwrite( STDERR, "$s\n" );
}

class mc
{
	/*
	 * Formats the code as text
	 */
	static function format( $code )
	{
		$term = array(
			'c_typedef',
			'c_structdef',
			'c_varlist'
		);
		$out = '';
		foreach( $code as $element ) {
			$out .= $element->format();
			$cn = get_class( $element );
			if( in_array( $cn, $term ) ) {
				$out .= ';';
			}
			$out .= "\n";
		}
		return $out;
	}
}

function mc_main($args)
{
	$home = getenv('CHE_HOME');
	if($home === false) {
		$home = '.';
	}
	define('MCDIR', $home);

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
	$cmd .= ' -fmax-errors=6 -D _XOPEN_SOURCE=700';
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
			 * from the referenced module
			 */
			$imp = get_import($t->path, $t->dir);
			foreach($imp->code as $decl) {
				if($decl instanceof c_typedef) {
					$s->add_type($decl->form->name);
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
		if($modname[0] != '.' && MCDIR == '.') {
			fwrite(STDERR, "CHE_HOME environment variable is not defined\n");
		}
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
				$imp->code[] = $element;
				break;
			case 'c_structdef':
				if(substr($element->name, 0, 2) != '__') {
					$imp->code[] = $element;
				}
				break;
			case 'c_enum':
				$imp->code[] = $element;
				break;
			case 'c_func':
				$dec = $element->proto;
				if($dec->type->l[0] != 'static') {
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
