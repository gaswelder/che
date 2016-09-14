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
		$code = mc::add_declarations( $code );

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
