<?php

parser::extend('operator', function (parser $parser) {
	if (!$parser->is_op($parser->s->peek())) {
		throw new ParseException("Not an operator");
	}
	return $parser->s->get()->type;
});

parser::extend('left-operator', function (parser $parser) {
	$L = array('&', '*', '++', '--', '!', '~');
	$t = $parser->s->get();
	if (!$t || !in_array($t->type, $L)) {
		throw new ParseException("Not a left-op");
	}
	return $t->type;
});

parser::extend('right-operator', function (parser $parser) {
	$r = ['--', '++'];
	$t = $parser->s->get();
	if (!$t || !in_array($t->type, $r)) {
		throw new ParseException("Not a right-op");
	}
	return ['op', $t->type];
});

parser::extend('index-op', function (parser $parser) {
	list($expr) = $parser->seq('[', '$che_expr', ']');
	return ['index', $expr];
});

parser::extend('struct-access-op', function (parser $parser) {
	try {
		list($id) = $parser->seq('.', '$che_identifier');
		return ['struct-access-dot', $id];
	} catch (ParseException $e) {
		//
	}

	list($id) = $parser->seq('->', '$che_identifier');
	return ['struct-access-arrow', $id];
});

parser::extend('call-op', function (parser $parser) {
	return che_function_call::parse($parser);
});

parser::extend('typecast', function (parser $parser) {
	list($tf) = $parser->seq('(', '$che_typeform', ')');
	return ['cast', $tf];
});

parser::extend('constant-expression', function (parser $parser) {
	return $parser->any(['literal', 'che_identifier']);
});
