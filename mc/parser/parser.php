<?php

require __DIR__ . '/buf.php';
require __DIR__ . '/lexer.php';
require __DIR__ . '/token.php';
require __DIR__ . '/subparsers.php';
require __DIR__ . '/literals.php';
require __DIR__ . '/expressions.php';
require __DIR__ . '/signatures.php';
require __DIR__ . '/control.php';

class ParseException extends Exception {}

// Second stage parser that returns CPOM objects
class parser
{
	public $s;

	private static $parsers = [];

	static function extend($name, $parser) {
		if (isset(self::$parsers[$name])) {
			throw new Exception("Parser '$name' already defined");
		}
		self::$parsers[$name] = $parser;
	}

	private $level = 0;

	private $type_names = array(
		'struct',
		'enum',
		'union',
		'void',
		'char',
		'short',
		'int',
		'long',
		'float',
		'double',
		'unsigned',
		'bool',
		'va_list',
		'FILE',
		'ptrdiff_t',
		'size_t',
		'wchar_t',
		'int8_t',
		'int16_t',
		'int32_t',
		'int64_t',
		'uint8_t',
		'uint16_t',
		'uint32_t',
		'uint64_t',
		'clock_t',
		'time_t',
		'fd_set',
		'socklen_t',
		'ssize_t'
	);

	public $path;

	function __construct($path)
	{
		$this->path = $path;
		$this->s = new mctok($path);
	}

	function __clone() {
		$this->s = clone $this->s;
		$this->level++;
	}

	function ended()
	{
		return $this->s->ended();
	}

	function error($msg)
	{
		$p = $this->s->peek();
		$pos = $p ? $p->pos : "EOF";
		$pos = $this->path.':'.$pos;
		fwrite(STDERR, "$pos: $msg\n");
		fwrite(STDERR, $this->context()."\n");
		exit(1);
	}

	function seq(...$names)
	{
		$backup = [clone $this->s, $this->type_names];

		try {
			$values = [];
			foreach ($names as $name) {
				if ($name[0] == '$') {
					$name = substr($name, 1);
					$values[] = $this->read($name);
				} else {
					$this->expect($name);
				}
			}
			return $values;
		} catch (ParseException $e) {
			$this->s = $backup[0];
			$this->type_names = $backup[1];
			throw $e;
		}
	}

	function many($name)
	{
		$list = [];
		while (1) {
			try {
				$list[] = $this->read($name);
			} catch (ParseException $e) {
				break;
			}
		}
		return $list;
	}

	function read($name)
	{
		$this->trace($name);
		$p = self::$parsers[$name];

		// Create an "alternative history" by spawning a copy of the parser.
		// Let it try to parse whatever it's parsing. If it fails with an exception,
		// this exception will simply bubble up to our caller. If it succeeds,
		// we assume the state of the cloned parser and return the result.
		$alt = clone $this;
		try {
			$obj = $p($alt);
		} catch (ParseException $e) {
			$this->trace("[$this->level] $name: " . $e->getMessage());
			throw $e;
		}
		$this->s = $alt->s;
		$this->type_names = $alt->type_names;
		$this->trace("[$this->level] $name: ok");
		return $obj;
	}

	function any($parsers)
	{
		foreach ($parsers as $parser) {
			try {
				return $this->read($parser);
			} catch (ParseException $e) {
				//
			}
		}
		throw new ParseException("Unknown input");
	}

	function trace($m)
	{
		$s = $this->context();
		$pref = str_repeat('  ', $this->level);
		trace::line("$pref $m |    $s");
	}

	function add_type($name)
	{
		if (!$name) {
			trigger_error("add_type: empty type name");
		}

		if (in_array($name, $this->type_names)) {
			trigger_error("add_type: redefinition of type $name");
		}
		$this->trace("New type: $name");
		$this->type_names[] = $name;
	}

	// Returns there is a type with the given name registered in the
	// current parser instance.
	function is_typename($name)
	{
		$ok = in_array($name, $this->type_names);
		$this->trace("is_typename($name) = ".($ok ? "yes" : "no"));
		return $ok;
	}

	/*
	 * Reads next program object from the source.
	 */
	function get()
	{
		if ($this->s->ended()) {
			return null;
		}
		return $this->read('root');
	}

	function context($n = 6)
	{
		$s = $this->s;

		$buf = array();
		while ($n-- > 0 && !$s->ended()) {
			$t = $s->get();
			$buf[] = $t;
		}

		$sbuf = array();
		foreach ($buf as $t) {
			$c = $t->content;
			if ($c === null) $c = $t->type;
			$sbuf[] = $c;
		}
		$str = implode(' ', $sbuf);

		while (!empty($buf)) {
			$s->unget(array_pop($buf));
		}
		return $str;
	}

	function is_op($t)
	{
		$ops = ['+', '-', '*', '/', '=', '|', '&', '~', '^', '<', '>', '?',
			':', '%', '+=', '-=', '*=', '/=', '%=', '&=', '^=', '|=', '++',
			'--', '->', '.', '>>', '<<', '<=', '>=', '&&', '||', '==', '!=',
			'<<=', '>>='];
		return in_array($t->type, $ops);
	}

	function expect($type, $content = null)
	{
		$t = $this->s->peek();
		if ($t->type != $type) {
			throw new ParseException("'$type' expected, got $t");
		}
		if ($content !== null && $t->content !== $content) {
			throw new ParseException("[$type, $content] expected, got $t");
		}
		return $this->s->get();
	}
}
