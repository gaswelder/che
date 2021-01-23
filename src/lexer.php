<?php

function read_file($path)
{
	return call_rust('read_file', $path);
}

function read($path)
{
	return read_file($path);
}

class lexer1
{
	private $proxy;

	function __construct($path)
	{
		$this->proxy = make_rust_object('lexer', $path);
	}

	function __call($f, $args)
	{
		return call_user_func_array([$this->proxy, $f], $args);
	}
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

	function peek()
	{
		return $this->peek_n(0);
	}

	function peek_n($n)
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
	return "[$token[kind], $c]";
}

function read_token($buf)
{
	return call_rust('read_token', $buf);
}
