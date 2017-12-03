<?php

// <expr>: <atom> [<op> <atom>]...
parser::extend('expr', function (parser $parser) {
	$expr = new c_expr();

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
});

parser::extend('operator', function(parser $parser) {
	if (!$parser->is_op($parser->s->peek())) {
		throw new ParseException("Not an operator");
	}
	return $parser->s->get()->type;
});

parser::extend('left-operator', function(parser $parser) {
	$L = array('&', '*', '++', '--', '!', '~');
	$t = $parser->s->get();
	if (!$t || !in_array($t->type, $L)) {
		throw new ParseException("Not a left-op");
	}
	return $t->type;
});

parser::extend('right-operator', function(parser $parser) {
	$r = ['--', '++'];
	$t = $parser->s->get();
	if (!$t || !in_array($t->type, $r)) {
		throw new ParseException("Not a right-op");
	}
	return ['op', $t->type];
});

parser::extend('index-op', function(parser $parser) {
	list ($expr) = $parser->seq('[', '$expr', ']');
	return ['index', $expr];
});

parser::extend('struct-access-op', function(parser $parser) {
	try {
		list ($id) = $parser->seq('.', '$identifier');
		return ['struct-access-dot', $id];
	} catch (ParseException $e) {
		//
	}

	list ($id) = $parser->seq('->', '$identifier');
	return ['struct-access-arrow', $id];
});

parser::extend('call-op', function(parser $parser) {
	$parser->expect('(');
	$args = [];
	while (1) {
		try {
			$args[] = $parser->read('expr');
		} catch (ParseException $e) {
			break;
		}
		try {
			$parser->expect(',');
		} catch (ParseException $e) {
			break;
		}
	}
	$parser->expect(')');
	return ['call', $args];
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
	while (1) {
		try {
			$ops[] = ['op', $parser->read('left-operator')];
		} catch (ParseException $e) {
			break;
		}
	}

	try {
		list ($expr) = $parser->seq('(', '$expr', ')');
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
	$parser->expect('sizeof');
	try {
		list ($arg) = $parser->seq('(', '$sizeof-contents', ')');
	} catch (ParseException $e) {
		list ($arg) = $parser->seq('$sizeof-contents');
	}
	return new c_sizeof($arg);
});

parser::extend('sizeof-contents', function(parser $parser) {
	return $parser->any(['typeform', 'expr']);
});

parser::extend('constant-expression', function(parser $parser) {
	return $parser->any(['literal', 'identifier']);
});

parser::extend('identifier', function(parser $parser) {
	$name = $parser->expect('word')->content;
	if ($parser->is_typename($name)) {
		throw new ParseException("typename, not id");
	}
	return new c_identifier($name);
});