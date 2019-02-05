<?php

class lexer_1
{
	/*
	 * Sorted by the length, longest first.
	 */
	private static $symbols = array(
		'<<=', '>>=', '...', '++',
		'--', '->', '<<', '>>',
		'<=', '>=', '&&', '||',
		'+=', '-=', '*=', '/=',
		'%=', '&=', '^=', '|=',
		'==', '!=', '!', '~',
		'&', '^', '*', '/',
		'%', '=', '|', ':',
		',', '<', '>', '+',
		'-', '{', '}', ';',
		'[', ']', '(', ')',
		'.', '?'
	);

	private static $keywords = array(
		'default',
		'typedef', 'struct',
		'import', 'union',
		'const', 'return',
		'switch', 'sizeof',
		'while', 'defer',
		'case', 'enum',
		'else', 'for',
		'pub', 'if'
	);

	const spaces = "\r\n\t ";

	private $s;

	function __construct($s)
	{
		$this->s = new buf($s);
	}

	function get()
	{
		return $this->read();
	}

	function error($msg)
	{
		$pos = $this->s->pos();
		$str = $this->s->context(10);
		return token::make('error', $msg, $pos);
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
			return token::make('macro', $s->skip_until("\n"), $pos);
		}

		/*
		 * Multiple-line comments.
		 */
		if ($s->skip_literal('/*')) {
			$comment = $s->until_literal('*/');
			if (!$s->skip_literal('*/')) {
				return $this->error("*/ expected");
			}
			return token::make('comment', $comment, $pos);
		}

		/*
		 * Singe-line comments.
		 */
		if ($s->skip_literal('//')) {
			$comment = $s->skip_until("\n");
			return token::make('comment', $comment, $pos);
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
				return token::make($word, null, $pos);
			}
			return token::make('word', $word, $pos);
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
			return token::make('string', $str, $pos);
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
			return token::make('char', $str, $pos);
		}

		/*
		 * Special character.
		 */
		$pos = $this->s->pos();
		foreach (self::$symbols as $sym) {
			if ($s->skip_literal($sym)) {
				return token::make($sym, null, $pos);
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
		return token::make('num', $num, $pos);
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

		return token::make('num', '0x' . $num, $pos);
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
