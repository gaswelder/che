<?php

class che_func extends che_element
{
	public $proto;
	public $body;

	function typenames()
	{
		$names = array_merge($this->proto->typenames(), $this->body->typenames());
		return $names;
	}

	function __construct($proto, $body)
	{
		$this->proto = $proto;
		$this->body = $body;
	}

	function format($tab = 0)
	{
		$s = $this->proto->format();
		$s = substr($s, 0, -1);
		// remove ';'
		$s .= ' ';
		$s .= $this->body->format();
		return $s;
	}

	function translate()
	{
		$f = clone $this;
		$n = count($f->body->parts);
		if (!$n) return $f;

		/*
		 * Make sure that every function's body ends with a return
		 * statement so that the deferred statements will be written
		 * there.
		 */
		if ($f->proto->form->name != 'main' && !($f->body->parts[$n - 1] instanceof che_return)) {
			$f->body->parts[] = new che_return();
		}

		$f->body = self::rewrite_body($f->body);
		return [
			$f->proto,
			$f
		];
	}

	private static function rewrite_body(che_body $b, $defer = array())
	{
		$c = new che_body();

		foreach ($b->parts as $part) {
			if ($part instanceof che_defer) {
				$defer[] = $part->expr;
				continue;
			}

			if ($part instanceof che_if || $part instanceof che_for || $part instanceof che_while) {
				$part = clone $part;
				$part->body = self::rewrite_body($part->body, $defer);
			} else if ($part instanceof che_switch) {
				$part = clone $part;
				foreach ($part->cases as $i => $case) {
					$part->cases[$i]->body = self::rewrite_body($part->cases[$i]->body, $defer);
				}
			} else if ($part instanceof che_return) {
				foreach ($defer as $e) {
					$c->parts[] = $e;
				}
			}
			$c->parts[] = $part;
		}

		return $c;
	}

	static function parse(parser $parser)
	{
		$pub = $parser->maybe('pub');
		$type = $parser->read('type');
		$form = $parser->read('form');

		if (empty($form->ops) || !($form->ops[0] instanceof che_formal_args)) {
			throw new ParseException("Not a function");
		}

		$args = array_shift($form->ops);
		$proto = new c_prototype($type, $form, $args);
		$proto->pub = $pub;

		$body = $parser->read('body');
		return new che_func($proto, $body);
	}
}
