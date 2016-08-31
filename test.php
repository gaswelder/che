<?php

require 'mc/mc.php';

require '/home/gas/code/php/lib/debug.php';
stop_on_error();

$paths = glob( "test/*" );
foreach( $paths as $path )
{
	echo "$path\n";
	$src = c_parse( $path );
	$str = c_format( $src );
	file_put_contents( 'out/'.basename( $path ), $str );
}

function c_format( $src ) {
	$s = '';
	foreach( $src as $tok ) {
		$s .= $tok->format() . "\n";
	}
	return $s;
}


function mc( $path )
{
	// Rewrite source file to the new one
	$newpath = str_replace( ' ', '-', "$path.mc.c" );
	$src = c_parse( $path );
	$str = c_format( $src );
	file_put_contents( $newpath, $str );

	// invoke cc
	exec( "c99 -Wall -Wextra -pedantic -pedantic-errors $newpath", $out, $ret );
}


?>
