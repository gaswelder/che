<?php

parser::extend('if', function(parser $parser) {
	$parser->s->expect('if');
	$parser->s->expect('(');
	$expr = $parser->read('expr');
	$parser->s->expect(')');

	$body = $parser->read('body-or-part');

	$t = $parser->s->peek();
	if ($t->type == 'else') {
		$parser->s->get();
		$else = $parser->read('body-or-part');
	}
	else {
		$else = null;
	}

	return new c_if($expr, $body, $else);
});

// <defer>: "defer" <expr>
parser::extend('defer', function(parser $parser) {
	$parser->s->expect('defer');
	return new c_defer($parser->read('expr'));
});

// <switch>: "switch" "(" <expr> ")" "{" <switch-case>... "}"
parser::extend('switch', function(parser $parser) {
	$parser->s->expect('switch');
	$parser->s->expect('(');
	$cond = $parser->read('expr');
	$parser->s->expect(')');

	$cases = [];

	$parser->s->expect('{');
	while (!$parser->s->ended() && $parser->s->peek()->type != '}') {
		$cases[] = $parser->read('switch-case');
	}
	$parser->s->expect('}');

	return new c_switch($cond, $cases);
});

// <switch-case>: "case" <literal>|<id> ":" <body-part>...
parser::extend('switch-case', function(parser $parser) {
	$parser->s->expect('case');

	// <id> | <literal>
	$peek = $parser->s->peek();
	if ($peek->type == 'word') {
		$val = new c_literal($parser->s->get()->content);
	}
	else {
		$val = $parser->read('literal');
	}

	// :
	$parser->s->expect(':');

	// <body-part>...
	$body = new c_body();
	while (!$parser->s->ended()) {
		$t = $parser->s->peek();
		if ($t->type == 'case') {
			break;
		}
		if ($t->type == '}') {
			break;
		}
		$body->add($parser->read('body-part'));
	}
	return array($val, $body);
});

parser::extend('while', function(parser $parser) {
	$parser->s->expect('while');
	$parser->s->expect('(');
	$cond = $parser->read('expr');
	$parser->s->expect(')');

	$body = $parser->read('body');

	return new c_while($cond, $body);
});

parser::extend('for', function(parser $parser) {
	$parser->s->expect('for');
	$parser->s->expect('(');

	if ($parser->type_follows()) {
		$init = $parser->read('varlist');
	}
	else {
		$init = $parser->read('expr');
	}
	$parser->s->expect(';');
	$cond = $parser->read('expr');
	$parser->s->expect(';');
	$act = $parser->read('expr');
	$parser->s->expect(')');

	$body = $parser->read('body');

	return new c_for($init, $cond, $act, $body);
});