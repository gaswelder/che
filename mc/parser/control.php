<?php

parser::extend('if', function(parser $parser) {
	return $parser->any(['if-else', 'if-only']);
});

parser::extend('if-else', function(parser $parser) {
	list($expr, $ok, $else) = $parser->seq('if', '(', '$expr', ')', '$body-or-part', 'else', '$body-or-part');
	return new c_if($expr, $ok, $else);
});

parser::extend('if-only', function(parser $parser) {
	list($expr, $body) = $parser->seq('if', '(', '$expr', ')', '$body-or-part');
	return new c_if($expr, $body, null);
});

// <defer>: "defer" <expr>
parser::extend('defer', function(parser $parser) {
	list($expr) = $parser->seq('defer', '$expr', ';');
	return new c_defer($expr);
});

// <switch>: "switch" "(" <expr> ")" "{" <switch-case>... "}"
parser::extend('switch', function(parser $parser) {
	list ($cond) = $parser->seq('switch', '(', '$expr', ')', '{');

	$cases = [];
	while (!$parser->s->ended() && $parser->s->peek()->type != '}') {
		$cases[] = $parser->read('switch-case');
	}
	$parser->expect('}');
	return new c_switch($cond, $cases);
	
});

// <switch-case>: "case" <literal>|<id> ":" <body-part>...
parser::extend('switch-case', function(parser $parser) {
	$parser->expect('case');

	// <id> | <literal>
	$val = $parser->any(['identifier', 'literal']);
	$parser->expect(':');


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
	list ($cond, $body) = $parser->seq('while', '(', '$expr', ')', '$body');
	return new c_while($cond, $body);
});

parser::extend('for', function(parser $parser) {
	return c_for::parse($parser);
});

// <return>: "return" [<expr>] ";"
parser::extend('return', function(parser $parser) {
	return $parser->any(['return-expr', 'return-empty']);
});

parser::extend('return-empty', function(parser $parser) {
	$parser->seq('return', ';');
	return new c_return();
});

parser::extend('return-expr', function(parser $parser) {
	list ($expr) = $parser->seq('return', '$expr', ';');
	return new c_return($expr->parts);
});
