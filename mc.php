<?php
require 'mc/mc.php';

exit(main($argv));

function main( $args )
{
	array_shift( $args );
	$path = array_shift( $args );

	$src = mc::rewrite( $path );
	file_put_contents( 'tmp.c', $src );
	$outname = str_replace( '.c', '', basename( $path ) );
	exec( 'pc tmp.c -o ' . $outname, $out, $ret );
	var_dump( $out, $ret );
}


?>
