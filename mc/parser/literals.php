<?php

parser::extend('literal', function (parser $parser) {
	return $parser->any([
		'che_number',
		'literal-string',
		'literal-char',
		'che_struct_literal',
		'array-literal',
		//'addr-literal',
		//'word-literal'
	]);
});

parser::extend('literal-string', function (parser $parser) {
	return new che_string($parser->expect('string')->content);
});

parser::extend('literal-char', function (parser $parser) {
	return new che_char($parser->expect('char')->content);
});

parser::extend('word-literal', function (parser $parser) {
	$t = $parser->expect('word');
	return new che_literal($t->content);
});

parser::extend('struct-literal-element', function (parser $parser) {
	list($id, $val) = $parser->seq('.', '$che_identifier', '=', '$che_expr');
	return new che_struct_literal_element($id, $val);
});

parser::extend('array-literal', function (parser $parser) {
	return che_array::parse($parser);
});

parser::extend('array-literal-element', function (parser $parser) {
	return $parser->any([
		'constant-expression',
		'designated-array-element'
	]);
});

parser::extend('designated-array-element', function (parser $parser) {
	return che_designated_array_element::parse($parser);
});
