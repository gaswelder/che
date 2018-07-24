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
		'c_typedef',
		'comment',
		'che_import',
		'enum-def',
		'struct-def-root',
		'function',
		'object-def'
	]);
});

parser::extend('comment', function (parser $parser) {
	return c_comment::parse($parser);
});

parser::extend('struct-def-root', function (parser $parser) {
	return c_structdef::parse($parser);
});

parser::extend('struct-identifier', function (parser $parser) {
	return c_struct_identifier::parse($parser);
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
			return new c_comment("#type $name");
		default:
			return $parser->error("Unknown macro: $type");
	}
});

parser::extend('struct-def-element', function (parser $parser) {
	$list = $parser->any([
		'embedded-union',
		'varlist'
	]);
	$parser->expect(';');
	return $list;
});

parser::extend('embedded-union', function (parser $parser) {
	list($u, $form) = $parser->seq('$union', '$form');
	$type = new c_type(array($u));
	$list = new che_varlist($type);
	$list->add($form);
	return $list;
});

parser::extend('assignment', function (parser $parser) {
	return $parser->seq('$form', '=', '$expr');
});


parser::extend('varlist', function (parser $parser) {
	$type = $parser->read('type');
	$list = new che_varlist($type);

	$next = $parser->any(['assignment', 'form']);
	$list->add($next);

	while (1) {
		try {
			$parser->expect(',');
		} catch (ParseException $e) {
			break;
		}

		try {
			$next = $parser->any(['assignment', 'form']);
			$list->add($next);
		} catch (ParseException $e) {
			break;
		}
	}
	return $list;
});

parser::extend('body', function (parser $parser) {
	return c_body::parse($parser);
});

parser::extend('body-expr', function (parser $parser) {
	$expr = $parser->read('expr');
	$parser->expect(';');
	return $expr;
});

parser::extend('body-part', function (parser $parser) {
	return $parser->any([
		'comment',
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
	list($list) = $parser->seq('$varlist', ';');
	return $list;
});

parser::extend('body-or-part', function (parser $parser) {
	try {
		return $parser->read('body');
	} catch (ParseException $e) {
		//
	}

	$b = new c_body();
	$b->add($parser->read('body-part'));
	return $b;
});

parser::extend('enum-def', function (parser $parser) {
	return c_enum::parse($parser);
});

parser::extend('enum-item', function (parser $parser) {
	return c_enum_item::parse($parser);
});

parser::extend('function', function (parser $parser) {
	return c_func::parse($parser);
});

parser::extend('object-def', function (parser $parser) {
	$type = $parser->read('type');
	$form = $parser->read('form');

	if (!empty($form->ops) && $form->ops[0] instanceof c_formal_args) {
		throw new ParseException("A function, not an object");
	}

	$expr = null;
	try {
		list($expr) = $parser->seq('=', '$expr');
	} catch (ParseException $e) {
		//
	}

	$parser->expect(';');
	$list = new che_varlist($type);
	$list->add([$form, $expr]);
	return $list;
});
