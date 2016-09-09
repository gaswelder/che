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

class mc
{
	static function rewrite( $path )
	{
		$body = "";
		$headers = array();
		$prototypes = '';
		$types = '';

		$src = c_parse( $path );
		foreach( $src as $element )
		{
			if( $element instanceof c_typedef ) {
				$types .= $element->format() . "\n";
				continue;
			}

			if( $element instanceof c_func ) {
				self::rewrite_func( $element, $headers );
				$prototypes .= $element->proto->format() . "\n";
			}
			$body .= $element->format();
		}

		$out = "// mc\n";

		$headers = array_keys( $headers );
		sort( $headers );
		foreach( $headers as $h ) {
			$out .= "#include <$h.h>\n";
		}
		$out .= "\n";

		$out .= $types . "\n";
		$out .= $prototypes . "\n";
		$out .= $body;
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
