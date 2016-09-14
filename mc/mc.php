<?php

require '/home/gas/code/php/lib/debug.php';
stop_on_error();
error_reporting( -1 );

$dir = dirname( __FILE__ );

require $dir . '/parsers/main.php';
require $dir . '/ctok1.php';
require $dir . '/ctok.php';
require $dir . '/cpom.php';
require $dir . '/stdc.php';

class mc
{
	/*
	 * Parses given file and returns array of code elements
	 */
	static function parse( $path )
	{
		$code = array();

		$f = new fstream( fopen( $path, 'rb' ) );
		$t1 = new ctok1( $f );
		$s = new ctok( $t1, $path );

		while( !$s->ended() ) {
			$t = $s->get();
			$code[] = $t;
		}
		return $code;
	}

	/*
	 * Formats the code as text
	 */
	static function format( $code )
	{
		$out = '';
		foreach( $code as $element ) {
			$out .= $element->format() . "\n";
		}
		return $out;
	}

	/*
	 * Takes parsed code tree and adds
	 * forward declarations and headers.
	 */
	static function add_declarations( $src )
	{
		$prototypes = array();
		$types = array();
		$body = array();
		$headers = array();

		foreach( $src as $element )
		{
			if( $element instanceof c_typedef ) {
				$types[] = $element;
				continue;
			}

			if( $element instanceof c_func ) {
				self::rewrite_func( $element, $headers );
				$prototypes[] = $element->proto;
			}

			$body[] = $element;
		}

		$out = array();

		$headers = array_keys( $headers );
		sort( $headers );
		foreach( $headers as $h ) {
			$out[] = new c_macro( 'include', "<$h.h>" );
		}

		$out = array_merge( $out, $types, $prototypes, $body );
		return $out;
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


?>
