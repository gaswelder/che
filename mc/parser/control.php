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

parser::extend('defer', function(parser $parser) {
	return c_defer::parse($parser);
});

parser::extend('switch', function(parser $parser) {
	return c_switch::parse($parser);
});

parser::extend('switch-case', function(parser $parser) {
	return c_switch_case::parse($parser);
});

parser::extend('while', function(parser $parser) {
	return c_while::parse($parser);
});

parser::extend('for', function(parser $parser) {
	return c_for::parse($parser);
});

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
