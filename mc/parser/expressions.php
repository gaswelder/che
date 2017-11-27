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
	try {
		$ops[] = $parser->read('typecast');
	} catch (ParseException $e) {
		//
	}

	try {
		$ops[] = ['literal', $parser->read('literal')];
		return $ops;
	} catch (ParseException $e) {
		//
	}

	try {
		$ops[] = ['sizeof', $parser->read('sizeof')];
		return $ops;
	} catch (ParseException $e) {
		//
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

parser::extend('typecast', function(parser $parser) {
	$parser->expect('(');
	if (!$parser->type_follows()) {
		throw new ParseException("Not a typecast");
	}
	$tf = $parser->read('typeform');
	$parser->expect(')');
	return ['cast', $tf];
});

// <sizeof>: "sizeof" <sizeof-arg>
// <sizeof-arg>: ("(" (<expr> | <type>) ")") | <expr> | <type>
parser::extend('sizeof', function(parser $parser) {
	$parts = [];

	$parser->expect('sizeof');

	$brace = false;
	if ($parser->s->peek()->type == '(') {
		$brace = true;
		$parser->s->get();
	}

	if ($parser->type_follows()) {
		$arg = $parser->read('typeform');
	}
	else {
		$arg = $parser->read('expr');
	}

	if ($brace) {
		$parser->expect(')');
	}

	return new c_sizeof($arg);
});
