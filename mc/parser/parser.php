<?php

require __DIR__ . '/lexer.php';
require __DIR__ . '/token.php';
require __DIR__ . '/subparsers.php';
require __DIR__ . '/literals.php';
require __DIR__ . '/expressions.php';

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

	function read($name)
	{
		$this->trace($name);
		$p = self::$parsers[$name];
		//return $p($this);

		// Create an "alternative history" by spawning a copy of the parser.
		// Let it try to parse whatever it's parsing. If it fails with an exception,
		// this exception will simply bubble up to our caller. If it succeeds,
		// we assume the state of the cloned parser and return the result.
		$alt = clone $this;
		$obj = $p($alt);
		$this->s = $alt->s;
		$this->type_names = $alt->type_names;
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
		return;
		$s = $this->context();
		$pref = str_repeat('  ', $this->level);
		fwrite(STDERR, "-- $pref $m\t|\t$s\n");
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

	//

	function form_follows()
	{
		if ($this->type_follows()) return false;
		$t = $this->s->peek()->type;
		return $t == '*' || $t == 'word';
	}

	function is_typename($word)
	{
		$ok = in_array($word, $this->type_names);
		$this->trace("is_typename($word) = ".($ok ? "yes" : "no"));
		return $ok;
		return in_array($word, $this->type_names);
	}

	function type_follows()
	{
		$b = $this->_type_follows();
		return $b;

		$r = $b ? '+' : '';
		$c = $this->context();
		put("$r	$c");

		return $b;
	}

	private function _type_follows()
	{
		$this->trace("type_follows");
		$t = $this->s->peek();

		$type_keywords = array('struct', 'const');
		if (in_array($t->type, $type_keywords)) {
			return true;
		}

		if ($t->type != 'word') {
			return false;
		}
		return $this->is_typename($t->content);
	}

	function context($n = 4)
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
