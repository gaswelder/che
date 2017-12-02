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
	return new c_char($parser->expect('char')->content);
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
		$val = $parser->read('expr');

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
	$parser->expect('{');

	$elements = [];
	while (1) {
		try {
			$elements[] = $parser->read('array-literal-element');
		} catch (ParseException $e) {
			break;
		}
		try {
			$parser->expect(',');
		} catch (ParseException $e) {
			break;
		}
	}
	$parser->expect('}');
	return new c_array($elements);
});

parser::extend('array-literal-element', function(parser $parser) {
	return $parser->any([
		'constant-expression',
		'designated-array-element'
	]);
});

parser::extend('designated-array-element', function(parser $parser) {
	$item = new c_designated_array_element;
	$parser->expect('[');
	try {
		$item->index = $parser->expect('word')->content;
	} catch (ParseException $e) {
		$item->index = $parser->expect('number')->content;
	}
	$parser->expect(']');
	$parser->expect('=');
	$item->value = $parser->read('constant-expression');
	return $item;
});

parser::extend('constant-expression', function(parser $parser) {
	return $parser->any(['literal', 'identifier']);
});

parser::extend('identifier', function(parser $parser) {
	$name = $parser->expect('word')->content;
	return new c_identifier($name);
});

// parser::extend('addr-literal', function(parser $parser) {
// 	$str = '&';
// 	$parser->expect('&');
// 	$str .= $parser->expect('word')->content;
// 	return new c_literal($str);
// });
