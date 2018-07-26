<?php

parser::extend('all', function (parser $parser) {
	$list = $parser->many('root');
	if (!$parser->ended()) {
		throw new Exception("Couldn't parse: " . $parser->context());
	}
	return $list;
});

// Root parser that operates on the top level of a module.
parser::extend('root', function (parser $parser) {
	return $parser->any([
		'macro',
		'che_typedef',
		'che_comment',
		'che_import',
		'che_enum',
		'che_structdef',
		'che_func',
		'root-varlist'
	]);
});

// <macro>: "#include" <string> | "#define" <name> <string>
// "#ifdef" <id> | "#ifndef" <id>
parser::extend('macro', function (parser $parser) {
	$m = $parser->expect('macro');

	$type = strtok($m->content, " \t");
	switch ($type) {
		case '#include':
			$path = trim(strtok("\n"));
			return new c_include($path);
		case '#define':
			$name = strtok("\t ");
			$value = strtok("\n");
			return new c_define($name, $value);
		case '#link':
			$name = strtok("\n");
			return new c_link($name);
		case '#ifdef':
		case '#ifndef':
			$id = trim(strtok("\n"));
			return new c_macro($type, $id);
		case '#endif':
			return new c_macro($type);
		case '#type':
			$name = trim(strtok("\n"));
			$parser->add_type($name);
			return new che_comment("#type $name");
		default:
			return $parser->error("Unknown macro: $type");
	}
});

parser::extend('body-expr', function (parser $parser) {
	$expr = $parser->read('che_expr');
	$parser->expect(';');
	return $expr;
});

parser::extend('body-part', function (parser $parser) {
	return $parser->any([
		'che_comment',
		'if',
		'for',
		'while',
		'switch',
		'return',
		'body-varlist',
		'defer',
		'body-expr'
	]);
});

parser::extend('body-varlist', function (parser $parser) {
	list($list) = $parser->seq('$che_varlist', ';');
	return $list;
});

parser::extend('body-or-part', function (parser $parser) {
	try {
		return $parser->read('che_body');
	} catch (ParseException $e) {
		//
	}

	$b = new che_body();
	$b->add($parser->read('body-part'));
	return $b;
});

parser::extend('root-varlist', function (parser $parser) {
	$varlist = $parser->read('object-def');
	// Mark all root variables static.
	$varlist->stat = true;
	return $varlist;
});

parser::extend('object-def', function (parser $parser) {
	$type = $parser->read('type');
	$form = $parser->read('che_form');

	if (!empty($form->ops) && $form->ops[0] instanceof che_formal_args) {
		throw new ParseException("A function, not an object");
	}

	$expr = null;
	try {
		list($expr) = $parser->seq('=', '$che_expr');
	} catch (ParseException $e) {
		//
	}

	$parser->expect(';');
	$list = new che_varlist($type);
	$list->add([$form, $expr]);
	return $list;
});
