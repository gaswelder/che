<?php

require 'mc/mc.php';

require '/home/gas/code/php/lib/debug.php';
stop_on_error();

$paths = glob( "test/*" );
foreach( $paths as $path )
{
	echo "$path\n";
	$src = mc::rewrite( $path );
	file_put_contents( 'out/'.basename( $path ), $src );
	//$src = c_parse( $path );
	//$str = c_format( $src );
}


class mc
{
	static function rewrite( $path )
	{
		$head = "// mc\n";
		$body = "";

		$headers = array();

		$src = c_parse( $path );
		foreach( $src as $element )
		{
			if( $element instanceof c_func )
			{
				self::rewrite_func( $element, $headers );
				/*

				add_headers( $element->body );

				for each identifier {
					$h = type header( $typename );
					if( $h ) {
						$headers->add( $h );
					}
				}
				*/

			}
			$body .= $element->format();
		}

		$headers = array_keys( $headers );
		sort( $headers );
		foreach( $headers as $h ) {
			$head .= "#include <$h.h>\n";
		}

		return $head . $body;
	}

	private static function rewrite_func( $func, &$headers )
	{
		$args = $func->proto->args;
		foreach( $args as $arg ) {
			if( !($arg instanceof c_nameform) ) continue;
			self::type( $arg->type, $headers );
		}

		self::body( $func->body, $headers );
	}

	private static function body( $body, &$headers )
	{
		foreach( $body->parts as $part )
		{
			$cn = get_class( $part );
			switch( $cn )
			{
				case 'c_vardef':
					self::type( $part->type, $headers );
					self::expr( $part->init, $headers );
					break;

				case 'c_if':
					self::expr( $part->cond, $headers );
					self::body( $part->body, $headers );
					if( $part->else ) {
						self::body( $part->else, $headers );
					}
					break;

				case 'c_while':
					self::expr( $part->cond, $headers );
					self::body( $part->body, $headers );
					break;

				case 'c_for':
					self::expr( $part->init, $headers );
					self::expr( $part->cond, $headers );
					self::expr( $part->act, $headers );
					self::body( $part->body, $headers );
					break;

				case 'c_switch':
					self::expr( $part->cond, $headers );
					foreach( $part->cases as $case ) {
						self::body( $case[1], $headers );
					}
					break;

				case 'c_return':
				case 'c_expr':
					self::expr( $part, $headers );
					break;

				default:
					var_dump( "1706", $part );
					exit;
			}
		}
	}

	private static function type( $type, &$headers )
	{
		foreach( $type as $cast ) {
			if( !is_string( $cast ) ) continue;
			self::id( $cast, $headers );
		}
	}

	private static function id( $id, &$headers )
	{
		$h = mc_clib::header( $id );
		if( $h ) $headers[$h] = true;
	}

	private static function expr( $e, &$headers )
	{
		if( !$e ) return;

		foreach( $e->parts as $part )
		{
			if( !is_array( $part ) ) continue;

			foreach( $part as $op ) {
				switch( $op[0] )
				{
					case 'id':
						self::id( $op[1], $headers );
						break;

					case 'call':
						foreach( $op[1] as $expr ) {
							self::expr( $expr, $headers );
						}
						break;

					case 'expr':
						self::expr( $op[1], $headers );
						break;

					case 'cast':
						self::type( $op[1], $headers );
						break;

					case 'literal':
					case 'op':
					case 'sizeof':
					case 'index':
						break;

					default:
						var_dump( $op );
						exit;
				}
			}
		}
	}
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
	$body = rewrite( $path );




	// Rewrite source file to the new one
	$newpath = str_replace( ' ', '-', "$path.mc.c" );

	$str = c_format( $src );
	file_put_contents( $newpath, $str );

	// invoke cc
	//exec( "c99 -Wall -Wextra -pedantic -pedantic-errors $newpath", $out, $ret );
}





?>
