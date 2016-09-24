<?php

// First stage parser

class token {
	public $type;
	public $content;
	public $pos;

	function __construct( $type, $content, $pos ) {
		$this->type = $type;
		$this->content = $content;
		$this->pos = $pos;
	}

	function __toString()
	{
		if( $this->content === null ) {
			return '[' . $this->type . ']';
		}

		$n = 40;
		if( mb_strlen( $this->content ) > $n ) {
			$c = mb_substr( $this->content, 0, $n-3 ) . '...';
		}
		else $c = $this->content;
		$c = str_replace( array( "\r", "\n", "\t" ),
			array( "\\r", "\\n", "\\t" ), $c );
		return "[$this->type, $c]";
	}
}

function tok( $type, $data, $pos ) {
	return new token( $type, $data, $pos );
}

class mctok
{
	const spaces = "\r\n\t ";

	private $s;

	/*
	 * Buffer for returned tokens.
	 */
	protected $buffer = array();

	function __construct( $path ) {
		$this->s = new buf(file_get_contents($path));
	}

	function ended() {
		return $this->peek() == null;
	}

	function get() {
		$t = $this->_get();
		return $t;
	}

	/*
	 * Returns next token, removing it from the stream.
	 */
	function _get()
	{
		if( !empty($this->buffer) ) {
			return array_pop($this->buffer);
		}

		$t = $this->read();
		while( $t && $t->type == 'comment' ) {
			$t = $this->read();
		}
		return $t;
	}

	/*
	 * Pushes a token back to the stream.
	 */
	function unget( $t ) {
		array_push( $this->buffer, $t );
	}

	/*
	 * Returns next token without removing it from the stream.
	 * Returns null if there is no next token.
	 */
	function peek( $n = 0 )
	{
		$buf = array();
		while( $n >= 0 )
		{
			$t = $this->get();
			if( $t === null ) {
				break;
			}

			$buf[] = $t;
			$n--;
		}

		if( $n == -1 ) {
			$r = $buf[count($buf)-1];
		}
		else {
			$r = null;
		}

		while( !empty( $buf ) ) {
			$this->unget( array_pop( $buf ) );
		}

		return $r;
	}

	/*
	 * Sorted by the length, longest first.
	 */
	private static $symbols = array(
		'<<=', '>>=',
		'...',
		'++', '--', '->', '<<', '>>', '<=', '>=', '&&', '||',
		'+=', '-=', '*=', '/=', '%=', '&=', '^=', '|=', '==', '!=',
		'!', '~', '&', '^', '*', '/', '%', '=', '|', ':', ',', '<', '>',
		'+', '-',
		'{', '}', ';', '[', ']', '(', ')', '.', '?'
	);

	private static $keywords = array(
		'typedef', 'struct', 'import', 'enum',
		'const', 'static',
		'if', 'else', 'for', 'while', 'return', 'switch', 'case',
		'sizeof'
	);

	private $newline = true;

	function error( $msg ) {
		$pos = $this->s->pos();
		$str = $this->s->context(10);
		//fwrite( STDERR, "$msg at $pos: \"$str...\"\n" );
		trigger_error( "$msg at $pos: \"$str...\"" );
		exit;
	}

	protected function _read() {
		$t = $this->_read();
		put( "read: $t" );
		return $t;
	}

	protected function read()
	{
		$s = $this->s;

		$sp = $s->read_set( self::spaces );
		if( strpos( $sp, "\n" ) !== false ) {
			$this->newline = true;
		}

		if( $s->ended() ) {
			return null;
		}

		$pos = $this->s->pos();

		/*
		 * If we are on a new line and '#' follows, read it as a macro.
		 */
		if( $this->newline && $s->peek() == '#' ) {
			return tok( 'macro', $s->skip_until( "\n" ), $pos );
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
			return tok( 'comment', $comment, $pos );
		}

		/*
		 * Singe-line comments.
		 */
		if( $s->skip_literal( '//' ) ) {
			$comment = $s->skip_until( "\n" );
			return tok( 'comment', $comment, $pos );
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
				return tok( $word, null, $pos );
			}
			return tok( 'word', $word, $pos );
		}

		/*
		 * Number
		 */
		if(ctype_digit($s->peek())) {
			return $this->read_number();
		}

		/*
		 * String.
		 */
		if( $s->peek() == '"' )
		{
			$str = '';
			/*
			 * A string literal may be split into parts,
			 * so concatenate it.
			 */
			while( $s->peek() == '"' ) {
				$str .= $this->read_string();
				$s->read_set( self::spaces );
			}
			return tok( 'string', $str, $pos );
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
			return tok( 'char', $str, $pos );
		}

		/*
		 * Special character.
		 */
		foreach( self::$symbols as $sym ) {
			if( $s->skip_literal( $sym ) ) {
				return tok( $sym, null, $pos );
			}
		}

		$ch = $s->peek();
		return $this->error( "Unexpected character: '$ch'" );
	}

	private function read_number()
	{
		$s = $this->s;
		$pos = $s->pos();

		$p1 = $s->get();
		$p2 = $s->peek();
		$s->unget( $p1 );

		if($p1 == '0' && $p2 == 'x') {
			$num = '0x';
			$s->get();
			$s->get();
			$num .= $s->read_set("0123456789ABCDEFabcdef");
		}
		else {
			$num = '';
			$num .= $s->read_set( '0123456789' );
		}
		if( $s->peek() == 'L' ) {
			$num .= $s->get();
		}
		return tok( 'num', $num, $pos );
	}

	private function read_string()
	{
		$s = $this->s;

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
		return $str;
	}
}

?>
