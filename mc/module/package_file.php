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
		$s = new mctok($this->path);
		while (1) {
			$t = $s->get();
			if ($t === null) {
				break;
			}
			if ($t->type != 'typedef') {
				continue;
			}
			 /*
			 * When a 'typedef' is encountered, look ahead
			 * to find the type name
			 */
			$name = self::get_typename($s);
			if (!$name) break;
			$list[] = $name;
		}
		return $list;
	}

	private static function get_typename(mctok $s)
	{
		 /*
		 * New type name is at the end of the typedef statement.
		 * It may have a value form or a function form.
		 */
 
		 /*
		 * Get all tokens until the semicolon.
		 */
		$buf = array();
		while (!$s->ended()) {
			$t = $s->get();
			if ($t->type == ';') {
				break;
			}
			$buf[] = $t;
		}

		if (empty($buf)) {
			trigger_error("No tokens after 'typedef'");
			return null;
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
			trigger_error("Type name expected in the typedef");
			return null;
		}
		return $name;
	}
}
