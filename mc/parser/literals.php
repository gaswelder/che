<?php

parser::extend('literal', function (parser $parser) {
	return $parser->any([
		'literal-number',
		'literal-string',
		'literal-char',
		'struct-literal',
		'array-literal',
		//'addr-literal',
		//'word-literal'
	]);
});

parser::extend('literal-number', function (parser $parser) {
	return che_number::parse($parser);
});

parser::extend('literal-string', function (parser $parser) {
	return new c_string($parser->expect('string')->content);
});

parser::extend('literal-char', function (parser $parser) {
	return new che_char($parser->expect('char')->content);
});

parser::extend('word-literal', function (parser $parser) {
	$t = $parser->expect('word');
	return new c_literal($t->content);
});

// <struct-literal>: "{" "." <id> "=" <literal> [, ...] "}"
parser::extend('struct-literal', function (parser $parser) {
	return c_struct_literal::parse($parser);
});

parser::extend('struct-literal-element', function (parser $parser) {
	list($id, $val) = $parser->seq('.', '$identifier', '=', '$expr');
	return new c_struct_literal_element($id, $val);
});

parser::extend('array-literal', function (parser $parser) {
	return c_array::parse($parser);
});

parser::extend('array-literal-element', function (parser $parser) {
	return $parser->any([
		'constant-expression',
		'designated-array-element'
	]);
});

parser::extend('designated-array-element', function (parser $parser) {
	return c_designated_array_element::parse($parser);
});
