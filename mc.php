<?php
define( 'MCDIR', '.' );
require 'mc/mc.php';

exit(main($argv));

function main( $args )
{
	array_shift( $args );
	$path = array_shift( $args );

	compile( array( $path ) );
}

function compile( $pipeline )
{
	/*
	 * Executable name
	 */
	$outname = basename( $pipeline[0] );
	if( $p = strrpos( $outname, '.' ) ) {
		$outname = substr( $outname, 0, $p );
	}

	/*
	 * C files that will be passed to the C compiler
	 */
	$sources = array();

	while( !empty( $pipeline ) )
	{
		/*
		 * Parse the module and translate it to C
		 */
		$path = array_shift($pipeline);
		$mod = parse_module($path);
		$code = mc_trans::translate($mod->code);

		/*
		 * Save the translated version and update the sources list
		 */
		$tmppath = "mcbuild/" . basename( $path ) . ".c";
		if( !file_exists( "mcbuild" ) && !mkdir( "mcbuild" ) ) {
			exit(1);
		}
		file_put_contents($tmppath, mc::format($code));
		$sources[] = $tmppath;
		$pipeline = array_merge($pipeline, $mod->deps);
	}

	exec( 'pc -g ' . implode( ' ', $sources ) . ' -o '.$outname, $output, $ret );
	//var_dump( $output, $ret );
	exit($ret);
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
			$imp = get_import($t->path);
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

	return $mod;
}

/*
 * Finds the given module, parses it
 * and returns an 'import' object.
 */
function get_import($modname)
{
	$path = find_import($modname);
	if(!$path) {
		fwrite( STDERR, "Could not find module: $mod\n" );
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
function find_import( $name )
{
	$p = array(
		$name . ".c",
		MCDIR . "/lib/$name.c"
	);
	foreach( $p as $path ) {
		if( file_exists( $path ) ) {
			return $path;
		}
	}
	return null;
}

?>
