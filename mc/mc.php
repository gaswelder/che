<?php

$path = '/home/gas/code/php';

error_reporting( -1 );

$dir = dirname( __FILE__ );

require $dir . '/parsers/main.php';
require $dir . '/ctok1.php';
require $dir . '/ctok.php';
require $dir . '/cpom.php';

function c_parse( $path )
{
	$src = array();
	
	$f = new fstream( fopen( $path, 'rb' ) );
	$t1 = new ctok1( $f );
	$s = new ctok( $t1, $path );

	while( !$s->ended() ) {
		$t = $s->get();
		//echo $t, "\n";
		//echo $f->pos(), ', ', $t1->pos(), ', ', $s->pos(), "\n";
		$src[] = $t;
		
		
		//echo $t->format(), "\n";
	}
	return $src;
}


?>
