<?php

function make_token($type, $data, $pos)
{
	return [
		'type' => $type,
		'content' => $data,
		'pos' => $pos
	];
}

function token_to_string($token)
{
	if ($token['content'] === null) {
		return '[' . $token['type'] . ']';
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
