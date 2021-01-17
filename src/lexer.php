<?php

require_once __DIR__ . '/buf.php';

function read($path)
{
	static $cache = [];

	if (!isset($cache[$path])) {
		$toks = [];
		$buf = new buf(file_get_contents($path));
		while (true) {
			$tok = read_token($buf);
			if (!$tok) break;
			if ($tok->type == 'error') {
				throw new Exception("$tok->content at $tok->pos");
			}
			$toks[] = $tok;
		}
		$cache[$path] = $toks;
	}
	return $cache[$path];
}

class lexer
{
	private $toks = [];

	function __construct($path)
	{
		if (!is_file($path)) {
			throw new Exception("can't parse '$path': not a file");
		}
		$this->toks = read($path);
	}

	function ended()
	{
		$t = $this->get();
		if ($t == null) {
			return true;
		}
		$this->unget($t);
		return false;
	}

	function more()
	{
		return !$this->ended();
	}

	function get()
	{
		while (true) {
			if (empty($this->toks)) {
				// var_dump('get', 'null');
				return null;
			}
			$tok = array_shift($this->toks);
			// var_dump('get', $tok);
			if ($tok->type != 'comment') return $tok;
		}
	}

	/*
	 * Pushes a token back to the stream.
	 */
	function unget($t)
	{
		array_unshift($this->toks, $t);
	}

	/*
	 * Returns next token without removing it from the stream.
	 * Returns null if there is no next token.
	 */
	function peek($n = 0)
	{
		if (count($this->toks) <= $n) {
			return null;
		}
		return $this->toks[$n];
	}

	function follows($type)
	{
		return $this->more() && $this->peek()->type == $type;
	}
}

function error_token($buf, $msg)
{
	return token::make('error', $msg, $buf->pos());
}

const spaces = "\r\n\t ";

function read_token($buf)
{
	$buf->read_set(spaces);

	if ($buf->ended()) {
		return null;
	}

	$pos = $buf->pos();

	/*
		 * If we are on a new line and '#' follows, read it as a macro.
		 */
	if ($buf->peek() == '#') {
		return token::make('macro', $buf->skip_until("\n"), $pos);
	}

	/*
		 * Multiple-line comments.
		 */
	if ($buf->skip_literal('/*')) {
		$comment = $buf->until_literal('*/');
		if (!$buf->skip_literal('*/')) {
			return error_token($buf, "*/ expected");
		}
		return token::make('comment', $comment, $pos);
	}

	/*
		 * Singe-line comments.
		 */
	if ($buf->skip_literal('//')) {
		$comment = $buf->skip_until("\n");
		return token::make('comment', $comment, $pos);
	}

	/*
		 * Identifier name.
		 */
	if (ctype_alpha($buf->peek()) || $buf->peek() == '_') {
		$word = '';
		while (!$buf->ended()) {
			$ch = $buf->peek();
			if (ctype_alpha($ch) || ctype_digit($ch) || $ch == '_') {
				$word .= $buf->get();
				continue;
			}
			break;
		}

		$keywords = array(
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
		if (in_array($word, $keywords)) {
			return token::make($word, null, $pos);
		}
		return token::make('word', $word, $pos);
	}

	/*
		 * Number
		 */
	if (ctype_digit($buf->peek())) {
		return read_number($buf);
	}

	/*
		 * String.
		 */
	if ($buf->peek() == '"') {
		$str = '';
		/*
			 * A string literal may be split into parts,
			 * so concatenate it.
			 */
		while ($buf->peek() == '"') {
			$str .= read_string($buf);
			$buf->read_set(spaces);
		}
		return token::make('string', $str, $pos);
	}

	/*
		 * Character literal
		 */
	if ($buf->peek() == "'") {
		$buf->get();

		$str = '';
		if ($buf->peek() == '\\') {
			$str .= $buf->get();
		}

		$str .= $buf->get();
		if ($buf->get() != "'") {
			return error_token($buf, "Single quote expected");
		}
		return token::make('char', $str, $pos);
	}

	$pos = $buf->pos();
	// Sorted by length, longest first.
	$symbols = array(
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
	foreach ($symbols as $sym) {
		if ($buf->skip_literal($sym)) {
			return token::make($sym, null, $pos);
		}
	}

	$ch = $buf->peek();
	return error_token($buf, "Unexpected character: '$ch'");
}

function read_number($buf)
{
	$pos = $buf->pos();

	// Peek at next 2 characters.
	$p1 = $buf->get();
	$p2 = $buf->peek();
	$buf->unget($p1);

	// If '0x' follows, read a hexademical constant.
	if ($p1 == '0' && $p2 == 'x') {
		return read_hex($buf);
	}

	$alpha = '0123456789';
	$num = $buf->read_set($alpha);

	$modifiers = ['U', 'L'];

	if ($buf->peek() == '.') {
		$modifiers = ['f'];
		$num .= $buf->get();
		$num .= $buf->read_set($alpha);
	}

	while (in_array($buf->peek(), $modifiers)) {
		$num .= $buf->get();
	}

	if (!$buf->ended() && ctype_alpha($buf->peek())) {
		$c = $buf->peek();
		return error_token($buf, "Unexpected character: '$c'");
	}
	return token::make('num', $num, $pos);
}

function read_hex($buf)
{
	$pos = $buf->pos();

	// Skip "0x"
	$buf->get();
	$buf->get();
	$alpha = '0123456789ABCDEFabcdef';

	$num = $buf->read_set($alpha);

	$modifiers = ['U', 'L'];

	while (in_array($buf->peek(), $modifiers)) {
		$num .= $buf->get();
	}

	return token::make('num', '0x' . $num, $pos);
}

function read_string($buf)
{
	$buf->get();
	$str = '';
	while (!$buf->ended() && $buf->peek() != '"') {
		$ch = $buf->get();
		$str .= $ch;
		if ($ch == '\\') {
			$str .= $buf->get();
		}
	}
	if ($buf->get() != '"') {
		return error_token($buf, "Double quote expected");
	}
	return $str;
}
