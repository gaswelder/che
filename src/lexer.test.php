<?php

require_once __DIR__ . '/buf.php';
require_once __DIR__ . '/lexer.php';

test('read_token', function () {
	eq([
		'type' => 'macro',
		'content' => '#type abc\n',
		'pos' => '1:1'
	], read_token(new buf('#type abc\n')));

	eq([
		'type' => 'comment',
		'content' => ' \ncomment /* comment \n',
		'pos' => '1:1'
	], read_token(new buf('/* \ncomment /* comment \n*/ 123')));

	eq([
		'type' => 'comment',
		'content' => ' comment',
		'pos' => '1:1'
	], read_token(new buf("// comment\n123")));

	$keywords = array(
		'default',
		'typedef', 'struct',
		'import', 'union',
		'const', 'return',
		'switch', 'sizeof',
		'while', 'defer',
		'case', 'enum',
		'else', 'for',
		'pub', 'if'
	);

	foreach ($keywords as $kw) {
		eq([
			'type' => $kw,
			'content' => null,
			'pos' => '1:1'
		], read_token(new buf("$kw 123")));
	}

	eq([
		'type' => 'word',
		'content' => 'abc_123',
		'pos' => '1:1'
	], read_token(new buf('abc_123++')));

	eq([
		'type' => 'string',
		'content' => 'abcdef',
		'pos' => '1:1'
	], read_token(new buf("\"abc\"\n\"def\" 123")));

	eq([
		'type' => 'char',
		'content' => 'a',
		'pos' => '1:1'
	], read_token(new buf("'a'123")));

	eq([
		'type' => 'char',
		'content' => '\\t',
		'pos' => '1:1'
	], read_token(new buf("'\\t'123")));

	$symbols = array(
		'<<=', '>>=', '...', '++',
		'--', '->', '<<', '>>',
		'<=', '>=', '&&', '||',
		'+=', '-=', '*=', '/=',
		'%=', '&=', '^=', '|=',
		'==', '!=', '!', '~',
		'&', '^', '*', '/',
		'%', '=', '|', ':',
		',', '<', '>', '+',
		'-', '{', '}', ';',
		'[', ']', '(', ')',
		'.', '?'
	);

	foreach ($symbols as $sym) {
		eq([
			'type' => $sym,
			'content' => null,
			'pos' => '1:1'
		], read_token(new buf($sym . '123')));
	}
});

test('read_number', function () {
	eq([
		'type' => 'num',
		'content' => '0x123',
		'pos' => '1:1'
	], read_number(new buf('0x123 abc')));

	eq([
		'type' => 'num',
		'content' => '123',
		'pos' => '1:1'
	], read_number(new buf('123 abc')));

	eq([
		'type' => 'num',
		'content' => '123UL',
		'pos' => '1:1'
	], read_number(new buf('123UL abc')));

	eq([
		'type' => 'num',
		'content' => '123.45',
		'pos' => '1:1'
	], read_number(new buf('123.45 abc')));
});

test('read_hex', function () {
	eq([
		'type' => 'num',
		'content' => '0x123',
		'pos' => '1:1'
	], read_hex(new buf('0x123')));

	eq([
		'type' => 'num',
		'content' => '0x123UL',
		'pos' => '1:1'
	], read_hex(new buf('0x123UL')));
});

test('read_string', function () {
	$buf = new buf('"abc" 123');
	eq('abc', read_string($buf));

	$buf = new buf('"abc\\"def" 123');
	eq('abc\"def', read_string($buf));
});
