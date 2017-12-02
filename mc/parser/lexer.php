<?php

class mctok
{
	const spaces = "\r\n\t ";

	private $s;

	/*
	 * Buffer for returned tokens.
	 */
	private $buffer = array();

	function __construct($path)
	{
		$this->s = new buf(file_get_contents($path));
	}

	function __clone()
	{
		$this->s = clone $this->s;
	}

	function ended()
	{
		return $this->peek() == null;
	}

	function get()
	{
		$t = $this->_get();
		return $t;
	}

	/*
	 * Returns next token, removing it from the stream.
	 */
	function _get()
	{
		if (!empty($this->buffer)) {
			return array_pop($this->buffer);
		}

		$t = $this->read();
		while ($t && $t->type == 'comment') {
			$t = $this->read();
		}
		return $t;
	}

	/*
	 * Pushes a token back to the stream.
	 */
	function unget($t)
	{
		array_push($this->buffer, $t);
	}

	function expect($type, $content = null)
	{
		$t = $this->get();
		if ($t->type != $type) {
			$this->error("'$type' expected, got $t");
		}
		if ($content !== null && $t->content !== $content) {
			$this->error("[$type, $content] expected, got $t");
		}
		return $t;
	}

	/*
	 * Returns next token without removing it from the stream.
	 * Returns null if there is no next token.
	 */
	function peek($n = 0)
	{
		$buf = array();
		while ($n >= 0) {
			$t = $this->get();
			if ($t === null) {
				break;
			}

			$buf[] = $t;
			$n--;
		}

		if ($n == -1) {
			$r = $buf[count($buf)-1];
		}
		else {
			$r = null;
		}

		while (!empty($buf)) {
			$this->unget(array_pop($buf));
		}

		return $r;
	}

	/*
	 * Sorted by the length, longest first.
	 */
	private static $symbols = array(
		'<<=',
		'>>=',
		'...',
		'++',
		'--',
		'->',
		'<<',
		'>>',
		'<=',
		'>=',
		'&&',
		'||',
		'+=',
		'-=',
		'*=',
		'/=',
		'%=',
		'&=',
		'^=',
		'|=',
		'==',
		'!=',
		'!',
		'~',
		'&',
		'^',
		'*',
		'/',
		'%',
		'=',
		'|',
		':',
		',',
		'<',
		'>',
		'+',
		'-',
		'{',
		'}',
		';',
		'[',
		']',
		'(',
		')',
		'.',
		'?'
	);

	private static $keywords = array(
		'typedef',
		'struct',
		'union',
		'import',
		'enum',
		'const',
		'pub',
		'if',
		'else',
		'for',
		'while',
		'defer',
		'return',
		'switch',
		'case',
		'sizeof'
	);

	function error($msg)
	{
		$pos = $this->s->pos();
		$str = $this->s->context(10);
		//fwrite( STDERR, "$msg at $pos: \"$str...\"\n" );
		trigger_error("$msg at $pos: \"$str...\"");
		exit;
	}

	protected function _read()
	{
		$t = $this->_read();
		put("read: $t");
		return $t;
	}

	protected function read()
	{
		$s = $this->s;

		$s->read_set(self::spaces);

		if ($s->ended()) {
			return null;
		}

		$pos = $this->s->pos();

		/*
		 * If we are on a new line and '#' follows, read it as a macro.
		 */
		if ($s->peek() == '#') {
			return tok('macro', $s->skip_until("\n"), $pos);
		}

		/*
		 * Multiple-line comments.
		 */
		if ($s->skip_literal('/*')) {
			$comment = $s->until_literal('*/');
			if (!$s->skip_literal('*/')) {
				return $this->error("*/ expected");
			}
			return tok('comment', $comment, $pos);
		}

		/*
		 * Singe-line comments.
		 */
		if ($s->skip_literal('//')) {
			$comment = $s->skip_until("\n");
			return tok('comment', $comment, $pos);
		}

		/*
		 * Identifier name.
		 */
		if (ctype_alpha($s->peek()) || $s->peek() == '_') {
			$word = '';
			while (!$s->ended()) {
				$ch = $s->peek();
				if (ctype_alpha($ch) || ctype_digit($ch) || $ch == '_') {
					$word .= $s->get();
					continue;
				}
				break;
			}

			if (in_array($word, self::$keywords)) {
				return tok($word, null, $pos);
			}
			return tok('word', $word, $pos);
		}

		/*
		 * Number
		 */
		if (ctype_digit($s->peek())) {
			return $this->read_number();
		}

		/*
		 * String.
		 */
		if ($s->peek() == '"') {
			$str = '';
			/*
			 * A string literal may be split into parts,
			 * so concatenate it.
			 */
			while ($s->peek() == '"') {
				$str .= $this->read_string();
				$s->read_set(self::spaces);
			}
			return tok('string', $str, $pos);
		}

		/*
		 * Character literal
		 */
		if ($s->peek() == "'") {
			$s->get();

			$str = '';
			if ($s->peek() == '\\') {
				$str .= $s->get();
			}

			$str .= $s->get();
			if ($s->get() != "'") {
				return $this->error("Single quote expected");
			}
			return tok('char', $str, $pos);
		}

		/*
		 * Special character.
		 */
		foreach (self::$symbols as $sym) {
			if ($s->skip_literal($sym)) {
				return tok($sym, null, $pos);
			}
		}

		$ch = $s->peek();
		return $this->error("Unexpected character: '$ch'");
	}

	private function read_number()
	{
		$s = $this->s;
		$pos = $s->pos();

		/*
		 * Peek at next 2 characters.
		 */
		$p1 = $s->get();
		$p2 = $s->peek();
		$s->unget($p1);

		// If '0x' follows, read a hexademical constant.
		if ($p1 == '0' && $p2 == 'x') {
			return $this->read_hex();
		}

		$alpha = '0123456789';
		$num = $s->read_set($alpha);
		
		$modifiers = ['U', 'L'];

		if ($s->peek() == '.') {
			$modifiers = ['f'];
			$num .= $s->get();
			$num .= $s->read_set($alpha);
		}
		
		while (in_array($s->peek(), $modifiers)) {
			$num .= $s->get();
		}

		if (!$s->ended() && ctype_alpha($s->peek())) {
			$c = $s->peek();
			return $this->error("Unexpected character: '$c'");
		}
		return tok('num', $num, $pos);
	}
	
	private function read_hex()
	{
		$s = $this->s;
		$pos = $s->pos();

		// Skip "0x"
		$s->get();
		$s->get();
		$alpha = '0123456789ABCDEFabcdef';
		
		$num = $s->read_set($alpha);
		
		$modifiers = ['U', 'L'];
		
		while (in_array($s->peek(), $modifiers)) {
			$num .= $s->get();
		}

		return tok('num', '0x'.$num, $pos);
	}

	private function read_string()
	{
		$s = $this->s;

		$s->get();
		$str = '';
		while (!$s->ended() && $s->peek() != '"') {
			$ch = $s->get();
			$str .= $ch;
			if ($ch == '\\') {
				$str .= $s->get();
			}
		}
		if ($s->get() != '"') {
			return $this->error("Double quote expected");
		}
		return $str;
	}
}