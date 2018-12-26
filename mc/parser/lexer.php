<?php

require __DIR__ . '/lexer_1.php';

class lexer
{
	private $toks = [];

	function __construct($path)
	{
		$s = new lexer_1(file_get_contents($path));
		$this->toks = [];
		while (true) {
			$tok = $s->get();
			if (!$tok) break;
			if ($tok->type == 'error') {
				throw new Exception("$tok->content at $tok->pos");
			}
			$this->toks[] = $tok;
		}
	}

	function ended()
	{
		return count($this->toks) == 0;
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
}
