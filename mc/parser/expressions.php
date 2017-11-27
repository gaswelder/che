<?php

// <expr>: <atom> [<op> <atom>]...
parser::extend('expr', function (parser $parser) {
	$e = new c_expr();
	$e->add($parser->read('atom'));

	while (!$parser->s->ended() && $parser->is_op($parser->s->peek())) {
		$e->add($parser->s->get()->type);
		$e->add($parser->read('atom'));
	}

	return $e;
});

// <expr-atom>: <cast>? (
// 		<literal> / <sizeof> /
// 		<left-op>... ("(" <expr> ")" / <name>) <right-op>...
// )
parser::extend('atom', function (parser $parser) {
	$ops = array();

	// <cast>?
	if ($parser->cast_follows()) {
		$parser->s->expect('(');
		$tf = $parser->read('typeform');
		$parser->s->expect(')');
		$ops[] = array('cast', $tf);
	}

	// <literal>?
	if ($parser->literal_follows()) {
		$ops[] = array(
			'literal',
			$parser->read('literal')
		);
		return $ops;
	}

	// <sizeof>?
	if ($parser->s->peek()->type == 'sizeof') {
		$ops[] = array(
			'sizeof',
			$parser->read('sizeof')
		);
		return $ops;
	}

	// <left-op>...
	$L = array('&', '*', '++', '--', '!', '~');
	while (!$parser->s->ended() && in_array($parser->s->peek()->type, $L)) {
		$ops[] = array('op', $parser->s->get()->type);
	}

	// "(" <expr> ")" / <name>
	if ($parser->s->peek()->type == '(') {
		$parser->s->expect('(');
		$ops[] = array('expr', $parser->read('expr'));
		$parser->s->expect(')');
	}
	else {
		$t = $parser->s->expect('word');
		$ops[] = array('id', $t->content);
	}

	while (!$parser->s->ended()) {
		$peek = $parser->s->peek();
		if ($peek->type == '--' || $peek->type == '++') {
			$ops[] = array(
				'op',
				$parser->s->get()->type
			);
			continue;
		}

		if ($peek->type == '[') {
			$parser->s->get();
			$ops[] = array(
				'index',
				$parser->read('expr')
			);
			$parser->s->expect(']');
			continue;
		}

		if ($peek->type == '.' || $peek->type == '->') {
			$parser->s->get();
			$ops[] = array('op', $peek->type);
			$t = $parser->s->get();
			if ($t->type != 'word') {
				return $parser->error("Id expected, got $t");
			}
			$ops[] = array('id', $t->content);
			continue;
		}

		if ($peek->type == '(') {
			$parser->s->get();
			$args = [];
			while (!$parser->s->ended() && $parser->s->peek()->type != ')') {
				$args[] = $parser->read('expr');
				if ($parser->s->peek()->type == ',') {
					$parser->s->get();
				}
				else {
					break;
				}
			}
			$parser->s->expect(')');
			$ops[] = array('call', $args);
		}

		break;
	}

	return $ops;
});
