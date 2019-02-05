<?php

class package_file
{
	private $path;

	function __construct($path)
	{
		$this->path = $path;
	}

	function path()
	{
		return $this->path;
	}

	/**
	 * Returns true if this file suits the given OS.
	 * 
	 * @param string $os OS identifier, like "unix".
	 */
	function suits_system(string $os) : bool
	{
		// Get second extension from the filename.
		// For 'main.unix.c' this will return 'unix'.
		// For 'main.c' this will return ''.
		$ext = pathinfo(pathinfo($this->path, PATHINFO_FILENAME), PATHINFO_EXTENSION);

		return !$ext || $ext == $os;
	}

	/**
	 * Returns list of type names (as strings) declared in this file.
	 */
	function typenames() : array
	{
		$list = array();
		 /*
		 * Scan file tokens for 'typedef' keywords
		 */
		$s = new lexer($this->path);
		while (1) {
			$t = $s->get();
			if ($t === null) {
				break;
			}

			// When a 'typedef' is encountered, look ahead
			// to find the type name
			if ($t->type == 'typedef') {
				$list[] = self::get_typename($s);
			}

			if ($t->type == 'macro' && strpos($t->content, '#type') === 0) {
				$list[] = trim(substr($t->content, strlen('#type') + 1));
			}
		}
		return $list;
	}

	private static function get_typename(lexer $lexer)
	{
		// The type name is at the end of the typedef statement.
		// typedef foo bar;
		// typedef {...} bar;
		// typedef struct foo bar;

		$skip_brackets = function () use ($lexer, &$skip_brackets) {
			expect($lexer, '{');
			while ($lexer->more()) {
				if ($lexer->follows('{')) {
					$skip_brackets();
					continue;
				}
				if ($lexer->follows('}')) {
					break;
				}
				$lexer->get();
			}
			expect($lexer, '}');
		};

		if ($lexer->follows('{')) {
			$skip_brackets();
			$name = expect($lexer, 'word')->content;
			expect($lexer, ';');
			return $name;
		}
		 
 
		 /*
		 * Get all tokens until the semicolon.
		 */
		$buf = array();
		while (!$lexer->ended()) {
			$t = $lexer->get();
			if ($t->type == ';') {
				break;
			}
			$buf[] = $t;
		}

		if (empty($buf)) {
			throw new Exception("No tokens after 'typedef'");
		}

		$buf = array_reverse($buf);
 
		 /*
		 * We assume that function typedefs end with "(...)".
		 * In that case we omit that part.
		 */
		if ($buf[0]->type == ')') {
			while (!empty($buf)) {
				$t = array_shift($buf);
				if ($t->type == '(') {
					break;
				}
			}
		}
 
		 /*
		 * The last 'word' token is assumed to be the type name.
		 */
		$name = null;
		while (!empty($buf)) {
			$t = array_shift($buf);
			if ($t->type == 'word') {
				$name = $t->content;
				break;
			}
		}

		if (!$name) {
			throw new Exception("Type name expected in the typedef");
		}
		return $name;
	}
}
