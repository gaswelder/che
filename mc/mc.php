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


?>
