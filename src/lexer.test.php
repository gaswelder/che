<?php

require_once __DIR__ . '/buf.php';
require_once __DIR__ . '/lexer.php';

test('read_token', function () {
	eq([
		'kind' => 'macro',
		'content' => '#type abc\n',
		'pos' => '1:1'
	], read_token(new buf('#type abc\n')));

	eq([
		'kind' => 'comment',
		'content' => ' \ncomment /* comment \n',
		'pos' => '1:1'
	], read_token(new buf('/* \ncomment /* comment \n*/ 123')));

	eq([
		'kind' => 'comment',
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
			'kind' => $kw,
			'content' => null,
			'pos' => '1:1'
		], read_token(new buf("$kw 123")));
	}

	eq([
		'kind' => 'word',
		'content' => 'abc_123',
		'pos' => '1:1'
	], read_token(new buf('abc_123++')));

	eq([
		'kind' => 'char',
		'content' => 'a',
		'pos' => '1:1'
	], read_token(new buf("'a'123")));

	eq([
		'kind' => 'char',
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
			'kind' => $sym,
			'content' => null,
			'pos' => '1:1'
		], read_token(new buf($sym . '123')));
	}

	eq([
		'kind' => 'num',
		'content' => '0x123',
		'pos' => '1:1'
	], read_token(new buf('0x123 abc')));

	eq([
		'kind' => 'num',
		'content' => '123',
		'pos' => '1:1'
	], read_token(new buf('123 abc')));

	eq([
		'kind' => 'num',
		'content' => '123UL',
		'pos' => '1:1'
	], read_token(new buf('123UL abc')));

	eq([
		'kind' => 'num',
		'content' => '123.45',
		'pos' => '1:1'
	], read_token(new buf('123.45 abc')));

	eq([
		'kind' => 'num',
		'content' => '0x123',
		'pos' => '1:1'
	], read_token(new buf('0x123')));

	eq([
		'kind' => 'num',
		'content' => '0x123UL',
		'pos' => '1:1'
	], read_token(new buf('0x123UL')));

	eq([
		'kind' => 'string',
		'content' => 'abc',
		'pos' => '1:1'
	], read_token(new buf("\"abc\" 123")));

	eq([
		'kind' => 'string',
		'content' => 'ab\\"c',
		'pos' => '1:1'
	], read_token(new buf('"ab\\"c" 123')));

	eq([
		'kind' => 'string',
		'content' => 'abcdef',
		'pos' => '1:1'
	], read_token(new buf("\"abc\" \"def\" 123")));
});
