<?php

class che_expr extends che_element
{
	public $parts;

	function __construct($parts = array())
	{
		$this->parts = $parts;
	}

	function add($p)
	{
		assert($p != null);
		$this->parts[] = $p;
	}

	function format($tab = 0)
	{
		if (empty($this->parts)) {
			return '';
		}
		$p = str_repeat("\t", $tab);
		$n = count($this->parts);
		$i = 0;
		$s = '';
		if ($this->parts[0] == '-') {
			$s = '-';
			$i++;
		}
		while ($i < $n) {
			$a = $this->parts[$i];
			$s .= $a->format();
			$i++;
			if ($i < $n) {
				$s .= ' ' . $this->parts[$i] . ' ';
				$i++;
			}
		}
		return $p . $s;
	}

	static function parse(parser $parser)
	{
		$expr = new self();

		// An expression may start with a minus.
		try {
			$parser->expect('-');
			$expr->add('-');
		} catch (ParseException $e) {
			//
		}

		$expr->add($parser->read('atom'));

		while (!$parser->s->ended() && $parser->is_op($parser->s->peek())) {
			$expr->add($parser->read('operator'));
			$expr->add($parser->read('atom'));
		}

		return $expr;
	}
}

class che_expr_atom extends che_element
{
	public $a;

	function __construct($a)
	{
		$this->a = $a;
	}

	function format()
	{
		$s = '';
		foreach ($this->a as $i) {
			if ($i instanceof che_function_call) {
				$s .= $i->format();
				continue;
			}

			switch ($i[0]) {
				case 'id':
					$s .= $i[1]->format();
					break;
				case 'op':
					$s .= $i[1];
					break;
				case 'struct-access-dot':
					$s .= '.' . $i[1]->format();
					break;
				case 'struct-access-arrow':
					$s .= '->' . $i[1]->format();
					break;
				case 'literal':
					$s .= $i[1]->format();
					break;
				case 'index':
					$s .= '[' . $i[1]->format() . ']';
					break;
				case 'sizeof':
					$s .= $i[1]->format();
					break;
				case 'cast':
					$s .= '(';
					$s .= $i[1]->format();
					$s .= ')';
					break;
				case 'expr':
					$s .= '(' . $i[1]->format() . ')';
					break;
				default:
					var_dump($i);
					exit(1);
			}
		}
		return $s;
	}

	static function parse(parser $parser)
	{
		$ops = array();

		// <cast>?
		try {
			$ops[] = $parser->read('typecast');
		} catch (ParseException $e) {
			//
		}

		try {
			$ops[] = ['literal', $parser->read('literal')];
			return new che_expr_atom($ops);
		} catch (ParseException $e) {
			//
		}

		try {
			$ops[] = ['sizeof', $parser->read('che_sizeof')];
			return new che_expr_atom($ops);
		} catch (ParseException $e) {
			//
		}
	
		// <left-op>...
		while (1) {
			try {
				$ops[] = ['op', $parser->read('left-operator')];
			} catch (ParseException $e) {
				break;
			}
		}

		try {
			list($expr) = $parser->seq('(', '$expr', ')');
			$ops[] = ['expr', $expr];
		} catch (ParseException $e) {
			$ops[] = ['id', $parser->read('identifier')];
		}

		while (1) {
			try {
				$ops[] = $parser->any([
					'right-operator',
					'index-op',
					'struct-access-op',
					'call-op'
				]);
				continue;
			} catch (ParseException $e) {
				break;
			}
		}

		return new che_expr_atom($ops);
	}
}
