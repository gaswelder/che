<?php

// First stage parser

class ctok1 extends toksbase
{
	const spaces = "\r\n\t ";

	/*
	 * Sorted by the length, longest first.
	 */
	private static $symbols = array(
		'<<=', '>>=',
		'...',
		'++', '--', '->', '<<', '>>', '<=', '>=', '&&', '||',
		'+=', '-=', '*=', '/=', '%=', '&=', '^=', '|=', '==', '!=',
		'!', '~', '&', '*', '/', '%', '=', '|', ':', ',', '<', '>',
		'+', '-',
		'{', '}', ';', '[', ']', '(', ')', '.', '?'
	);

	private static $keywords = array(
		'typedef', 'struct', 'import',
		'const', 'static',
		'if', 'else', 'for', 'while', 'return', 'switch', 'case',
		'sizeof'
	);

	private $newline = true;

	function error( $msg ) {
		$pos = $this->upstream->pos();
		$str = $this->upstream->context( 20 );
		//fwrite( STDERR, "$msg at $pos: \"$str...\"\n" );
		trigger_error( "$msg at $pos: \"$str...\"" );
		exit;
	}

	function get() {
		$t = parent::get();
		while( $t && $t->type == 'comment' ) {
			$t = parent::get();
		}
		//put( "Get: $t" );
		return $t;
	}

	protected function _read() {
		$t = $this->_read();
		put( "read: $t" );
		return $t;
	}

	protected function read()
	{
		$s = $this->upstream;

		$sp = $s->read_set( self::spaces );
		if( strpos( $sp, "\n" ) !== false ) {
			$this->newline = true;
		}

		if( $s->ended() ) {
			return null;
		}

		/*
		 * If we are on a new line and '#' follows, read it as a macro.
		 */
		if( $this->newline && $s->peek() == '#' ) {
			return tok( 'macro', $s->skip_until( "\n" ) );
		}

		$this->newline = false;

		/*
		 * Multiple-line comments.
		 */
		if( $s->skip_literal( '/*' ) )
		{
			$comment = $s->until_literal( '*/' );
			if( !$s->skip_literal( '*/' ) ) {
				return $this->error( "*/ expected" );
			}
			return tok( 'comment', $comment );
		}

		/*
		 * Singe-line comments.
		 */
		if( $s->skip_literal( '//' ) ) {
			$comment = $s->skip_until( "\n" );
			return tok( 'comment', $comment );
		}

		/*
		 * Identifier name.
		 */
		if( ctype_alpha( $s->peek() ) || $s->peek() == '_' )
		{
			$word = '';
			while( !$s->ended() )
			{
				$ch = $s->peek();
				if( ctype_alpha( $ch ) || ctype_digit( $ch ) || $ch == '_' ) {
					$word .= $s->get();
					continue;
				}
				break;
			}

			if( in_array( $word, self::$keywords ) ) {
				return tok( $word );
			}
			return tok( 'word', $word );
		}

		/*
		 * Number
		 */
		$p1 = $s->get();
		$p2 = $s->peek();
		$s->unget( $p1 );
		if( ctype_digit( $p1 ) || $p1 == '-' && ctype_digit( $p2 ) ) {
			$num = '';
			if( $p1 == '-' ) {
				$num = $s->get();
			}
			$num .= $s->read_set( '0123456789' );
			if( $s->peek() == 'L' ) {
				$num .= $s->get();
			}
			return tok( 'num', $num );
		}

		/*
		 * String.
		 */
		if( $s->peek() == '"' ) {
			$s->get();
			$str = '';
			while( !$s->ended() && $s->peek() != '"' ) {
				$ch = $s->get();
				$str .= $ch;
				if( $ch == '\\' ) {
					$str .= $s->get();
				}
			}
			if( $s->get() != '"' ) {
				return $this->error( "Double quote expected" );
			}
			return tok( 'string', $str );
		}

		/*
		 * Character literal
		 */
		if( $s->peek() == "'" ) {
			$s->get();

			$str = '';
			if( $s->peek() == '\\' ) {
				$str .= $s->get();
			}

			$str .= $s->get();
			if( $s->get() != "'" ) {
				return $this->error( "Single quote expected" );
			}
			return tok( 'char', $str );
		}

		/*
		 * Special character.
		 */
		foreach( self::$symbols as $sym ) {
			if( $s->skip_literal( $sym ) ) {
				return tok( $sym );
			}
		}

		$ch = $s->peek();
		return $this->error( "Unexpected character: '$ch'" );
	}
}

?>
