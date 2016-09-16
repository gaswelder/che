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

	while( !empty( $pipeline ) ) {
		/*
		 * Convert MC source to C source
		 */
		$path = array_shift( $pipeline );
		$code = mc::parse( $path );
		$code = mc_headers::convert( $code );

		$n = count( $code );
		for( $i = 0; $i < $n; $i++ ) {
			if( !($code[$i] instanceof c_import) ) {
				continue;
			}

			/*
			 * Replace the import with imported declarations
			 */
			$mod = $code[$i]->path;
			$imppath = find_import( $mod );
			if( !$imppath ) {
				fwrite( STDERR, "Could not find module: $mod\n" );
				exit(1);
			}
			$decls = mc::import( $imppath );
			array_splice( $code, $i, 1, $decls );
			$i--;
			$n += count( $decls ) - 1;

			/*
			 * Add the imported module to the processing
			 */
			$pipeline[] = $imppath;
		}

		/*
		 * Save the converted C source and add it to
		 * the compiler's command
		 */
		$name = "mcbuild/" . basename( $path ) . ".c";
		if( !file_exists( "mcbuild" ) && !mkdir( "mcbuild" ) ) {
			exit(1);
		}
		file_put_contents( $name, mc::format( $code ) );
		$sources[] = $name;
	}

	exec( 'pc ' . implode( ' ', $sources ) . ' -o '.$outname, $output, $ret );
	var_dump( $output, $ret );
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
