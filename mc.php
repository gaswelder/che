<?php
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
			$imppath = $code[$i]->path . ".c";
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
		$name = basename( $path ) . '.c';
		file_put_contents( $name, mc::format( $code ) );
		$sources[] = $name;
	}

	exec( 'pc ' . implode( ' ', $sources ) . ' -o a.out', $output, $ret );
	var_dump( $output, $ret );
}

?>
