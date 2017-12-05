<?php

// Root parser that operates on the top level of a module.
parser::extend('root', function(parser $parser) {
	return $parser->any([
		'macro',
		'typedef',
		'comment',
		'import',
		'enum-def',
		'struct-def-root',
		'function',
		'object-def'
	]);
});

parser::extend('comment', function(parser $parser) {
	return c_comment::parse($parser);
});

parser::extend('import', function(parser $parser) {
	return c_import::parse($parser);
});

parser::extend('typedef', function(parser $parser) {
	return c_typedef::parse($parser);
});

parser::extend('struct-def-root', function(parser $parser) {
	return c_structdef::parse($parser);
});

parser::extend('struct-identifier', function(parser $parser) {
	return c_struct_identifier::parse($parser);
});

// <macro>: "#include" <string> | "#define" <name> <string>
// "#ifdef" <id> | "#ifndef" <id>
parser::extend('macro', function(parser $parser) {
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

parser::extend('struct-def-element', function(parser $parser) {
	if ($parser->s->peek()->type == 'union') {
		$u = $parser->read('union');
		$type = new c_type(array($u));
		$form = $parser->read('form');

		$list = new c_varlist($type);
		$list->add($form);
	}
	else {
		$list = $parser->read('varlist');
	}
	$parser->expect(';');
	return $list;
});

parser::extend('embedded-union', function(parser $parser) {
	list ($u, $form) = $parser->seq('$union', '$form');
	$type = new c_type(array($u));
	$list = new c_varlist($type);
	$list->add($form);
	return $list;
});

parser::extend('assignment', function(parser $parser) {
	return $parser->seq('$form', '=', '$expr');
});


parser::extend('varlist', function(parser $parser) {
	$type = $parser->read('type');
	$list = new c_varlist($type);

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

parser::extend('body', function(parser $parser) {
	return c_body::parse($parser);
});

parser::extend('body-expr', function(parser $parser) {
	$expr = $parser->read('expr');
	$parser->expect(';');
	return $expr;
});

parser::extend('body-part', function(parser $parser) {
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

parser::extend('body-varlist', function(parser $parser) {
	list ($list) = $parser->seq('$varlist', ';');
	return $list;
});

parser::extend('body-or-part', function(parser $parser) {
	if ($parser->s->peek()->type == '{') {
		return $parser->read('body');
	}

	$b = new c_body();
	$b->add($parser->read('body-part'));
	return $b;
});

parser::extend('enum-def', function(parser $parser) {
	return c_enum::parse($parser);
});

parser::extend('enum-item', function(parser $parser) {
	return c_enum_item::parse($parser);
});

parser::extend('function', function(parser $parser) {
	return c_func::parse($parser);
});

parser::extend('object-def', function(parser $parser) {
	$type = $parser->read('type');
	$form = $parser->read('form');

	if (!empty($form->ops) && $form->ops[0] instanceof c_formal_args) {
		throw new ParseException("A function, not an object");
	}
	
	$expr = null;
	if ($parser->s->peek()->type == '=') {
		$parser->s->get();
		$expr = $parser->read('expr');
	}

	$parser->expect(';');
	$list = new c_varlist($type);
	$list->add([$form, $expr]);
	return $list;
});
