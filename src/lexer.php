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
			if ($tok['kind'] == 'error') {
				throw new Exception($tok['content'] . ' at ' . $tok['pos']);
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
				return null;
			}
			$tok = array_shift($this->toks);
			if ($tok['kind'] != 'comment') return $tok;
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
		return $this->more() && $this->peek()['kind'] == $type;
	}
}

function error_token($buf, $msg)
{
	return make_token('error', $msg, $buf->pos());
}

function make_token($kind, $content, $pos)
{
	return call_rust_mem('make_token', $kind, $content, $pos);
}

function token_to_string($token)
{
	if ($token['content'] === null) {
		return '[' . $token['kind'] . ']';
	}

	$n = 40;
	if (mb_strlen($token['content']) > $n) {
		$c = mb_substr($token['content'], 0, $n - 3) . '...';
	} else $c = $token['content'];
	$c = str_replace(array("\r", "\n", "\t"), array(
		"\\r",
		"\\n",
		"\\t"
	), $c);
	return "[$token[type], $c]";
}

const spaces = "\r\n\t ";

function read_token($buf)
{
	$buf->read_set(spaces);

	if ($buf->ended()) {
		return null;
	}

	$pos = $buf->pos();

	if ($buf->peek() == '#') {
		return make_token('macro', $buf->skip_until("\n"), $pos);
	}

	if ($buf->literal_follows("/*")) {
		return read_multiline_comment($buf);
	}

	if ($buf->skip_literal('//')) {
		$comment = $buf->skip_until("\n");
		return make_token('comment', $comment, $pos);
	}

	if (ctype_alpha($buf->peek()) || $buf->peek() == '_') {
		return read_word($buf);
	}

	if (ctype_digit($buf->peek())) {
		return read_number($buf);
	}

	if ($buf->peek() == '"') {
		return read_string_literal($buf);
	}

	if ($buf->peek() == "'") {
		return read_char_literal($buf);
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
			return make_token($sym, null, $pos);
		}
	}

	$ch = $buf->peek();
	return error_token($buf, "Unexpected character: '$ch'");
}

function read_multiline_comment($buf)
{
	$pos = $buf->pos();
	$buf->skip_literal('/*');
	$comment = $buf->until_literal('*/');
	if (!$buf->skip_literal('*/')) {
		return error_token($buf, "*/ expected");
	}
	return make_token('comment', $comment, $pos);
}

function read_char_literal($buf)
{
	return call_rust('read_char_literal', $buf);
}

function read_word($buf)
{
	return call_rust('read_word', $buf);
}

function read_number($buf)
{
	return call_rust('read_number', $buf);
}

function read_hex($buf)
{
	return call_rust('read_hex', $buf);
}

function read_string_literal($buf)
{
	return call_rust('read_string_literal', $buf);
}
