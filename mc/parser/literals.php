<?php

parser::extend('literal', function(parser $parser) {
	$t = $parser->s->get();

	if ($t->type == 'num') {
		return new c_number($t->content);
	}

	if ($t->type == 'string') {
		return new c_string($t->content);
	}

	if ($t->type == 'char') {
		return new c_char($t->content);
	}

	if ($t->type == '-') {
		$n = $parser->s->expect('num');
		return new c_number('-'.$n->content);
	}

	if ($t->type == '{') {
		$peek = $parser->s->peek();
		$parser->s->unget($t);

		if ($peek->type == '.') {
			return $parser->read('struct-literal');
		}
		else {
			return $parser->read('array-literal');
		}
	}

	if ($t->type == 'word') {
		return new c_literal($t->content);
	}

	if ($t->type == '&') {
		$parser->s->unget($t);
		return $parser->read('addr-literal');
	}

	return $parser->error("Unexpected $t");
});

// <struct-literal>: "{" "." <id> "=" <literal> [, ...] "}"
parser::extend('struct-literal', function(parser $parser) {
	$struct = new c_struct_literal();

	$parser->s->expect('{');
	while (!$parser->s->ended()) {
		$parser->s->expect('.');
		$id = $parser->s->expect('word')->content;
		$parser->s->expect('=');
		$val = $parser->read('literal');

		$struct->add($id, $val);

		if ($parser->s->peek()->type == ',') {
			$parser->s->get();
		}
		else {
			break;
		}
	}
	$parser->s->expect('}');
	return $struct;
});

parser::extend('array-literal', function(parser $parser) {
	$elements = array();

	$parser->s->expect('{');
	while (!$parser->s->ended() && $parser->s->peek()->type != '}') {
		$elements[] = $parser->read('literal');
		if ($parser->s->peek()->type == ',') {
			$parser->s->get();
		}
	}
	$parser->s->expect('}');

	return new c_array($elements);
});

parser::extend('addr-literal', function(parser $parser) {
	$str = '&';
	$parser->s->expect('&');
	$str .= $parser->s->expect('word')->content;
	return new c_literal($str);
});
