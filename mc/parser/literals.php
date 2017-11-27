<?php

parser::extend('literal', function(parser $parser) {
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

parser::extend('literal-number', function(parser $parser) {
	// Optional minus sign.
	$s = '';
	try {
		$parser->expect('-');
		$s = '-';
	} catch (ParseException $e) {
		//
	}
	// The number token.
	$s .= $parser->expect('num')->content;
	return new c_number($s);
});

parser::extend('literal-string', function(parser $parser) {
	return new c_string($parser->expect('string')->content);
});

parser::extend('literal-char', function(parser $parser) {
	return new c_char($parser->expect('chat')->content);
});

parser::extend('word-literal', function(parser $parser) {
	$t = $parser->expect('word');
	return new c_literal($t->content);
});

// <struct-literal>: "{" "." <id> "=" <literal> [, ...] "}"
parser::extend('struct-literal', function(parser $parser) {
	$struct = new c_struct_literal();

	$parser->expect('{');
	while (!$parser->s->ended()) {
		$parser->expect('.');
		$id = $parser->expect('word')->content;
		$parser->expect('=');
		$val = $parser->read('literal');

		$struct->add($id, $val);

		if ($parser->s->peek()->type == ',') {
			$parser->s->get();
		}
		else {
			break;
		}
	}
	$parser->expect('}');
	return $struct;
});

parser::extend('array-literal', function(parser $parser) {
	$elements = array();

	$parser->expect('{');
	while (!$parser->s->ended() && $parser->s->peek()->type != '}') {
		$elements[] = $parser->read('literal');
		if ($parser->s->peek()->type == ',') {
			$parser->s->get();
		}
	}
	$parser->expect('}');

	return new c_array($elements);
});

// parser::extend('addr-literal', function(parser $parser) {
// 	$str = '&';
// 	$parser->expect('&');
// 	$str .= $parser->expect('word')->content;
// 	return new c_literal($str);
// });
