<?php

class lexer
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
