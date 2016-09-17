<?php

require '/home/gas/code/php/lib/debug.php';
stop_on_error();
error_reporting( -1 );

$dir = dirname( __FILE__ );

require $dir . '/buf.php';
require $dir . '/tokens.php';
require $dir . '/parser.php';
require $dir . '/objects.php';
require $dir . '/stdc.php';

function trace( $m, $s = null ) {
	fwrite( STDERR, "--- $m		$s\n" );
}

function put( $s ) {
	fwrite( STDERR, "$s\n" );
}

class mc
{
	/*
	 * Parses given file and returns array of code elements
	 */
	static function parse( $path )
	{
		$code = array();

		$s = new parser( $path );

		while( !$s->ended() ) {
			$t = $s->get();
			//echo $t, "\n";
			$code[] = $t;
		}
		return $code;
	}

	/*
	 * Formats the code as text
	 */
	static function format( $code )
	{
		$term = array( 'c_vardef' );
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

	static function import( $path )
	{
		$code = self::parse( $path );
		$decls = array();
		foreach( $code as $element )
		{
			$cn = get_class( $element );

			switch( $cn ) {
				case 'c_typedef':
				case 'c_structdef':
				case 'c_enum':
					$decls[] = $element;
					break;
				case 'c_func':
					$dec = $element->proto;
					if( !in_array( 'static', $dec->type ) ) {
						$decls[] = $dec;
					}
					break;
			}
		}
		return $decls;
	}
}


?>
