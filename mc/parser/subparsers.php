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
	$t = $parser->expect('comment');
	return new c_comment($t->content);
});

parser::extend('import', function(parser $parser) {
	list ($path) = $parser->seq('import', '$literal-string');
	$dir = dirname(realpath($parser->path));
	return new c_import($path->content, $dir);
});


// <typedef>: "typedef" <type> <form> ";"
parser::extend('typedef', function(parser $parser) {
	list($type, $form) = $parser->seq('typedef', '$type', '$form', ';');
	$parser->add_type($form->name);
	return new c_typedef($type, $form);
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

parser::extend('struct-def-root', function(parser $parser) {
	$pub = false;
	try {
		$parser->expect('pub');
		$pub = true;
	} catch (ParseException $e) {
		//
	}

	list ($id) = $parser->seq('struct', '$identifier', '{');
	$lists = $parser->many('struct-def-element');
	$parser->seq('}', ';');

	$def = new c_structdef($id->content, $lists);
	$def->pub = $pub;
	return $def;
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

// <body>: "{" <body-part>... "}"
parser::extend('body', function(parser $parser) {
	$body = new c_body();

	$parser->expect('{');
	while (!$parser->s->ended() && $parser->s->peek()->type != '}') {
		$parserart = $parser->read('body-part');
		$body->add($parserart);
	}
	$parser->expect('}');
	return $body;
});

// <body-part>: (comment | <obj-def> | <construct>
// 	| (<expr> ";") | (<defer> ";") | <body> )...
parser::extend('body-part', function(parser $parser) {
	try {
		return $parser->any([
			'comment',
			'if',
			'for',
			'while',
			'switch',
			'return',
			'body-varlist',
			'defer',
			//'expr'
		]);
	} catch (ParseException $e) {
		//
	}

	$expr = $parser->read('expr');
	$parser->s->expect(';');
	return $expr;
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

// <enum-def>: "pub"? "enum" "{" <id> ["=" <literal>] [,]... "}" ";"
parser::extend('enum-def', function(parser $parser) {
	$pub = false;
	try {
		$parser->expect('pub');
		$pub = true;
	} catch (ParseException $e) {
		//
	}

	$parser->expect('enum');
	$parser->expect('{');

	$enum = new c_enum();
	$enum->pub = $pub;
	while (1) {
		$id = $parser->expect('word')->content;
		$val = null;
		if ($parser->s->peek()->type == '=') {
			$parser->s->get();
			$val = $parser->read('literal');
		}
		$enum->add($id, $val);
		if ($parser->s->peek()->type == ',') {
			$parser->s->get();
		}
		else {
			break;
		}
	}
	$parser->expect('}');
	$parser->expect(';');
	return $enum;
});

parser::extend('function', function(parser $parser) {
	$pub = false;
	try {
		$parser->expect('pub');
		$pub = true;
	} catch (ParseException $e) {
		//
	}

	$type = $parser->read('type');
	$form = $parser->read('form');

	if (empty($form->ops) || !($form->ops[0] instanceof c_formal_args)) {
		throw new ParseException("Not a function");
	}

	$args = array_shift($form->ops);
	$proto = new c_prototype($type, $form, $args);
	$proto->pub = $pub;

	$body = $parser->read('body');
	return new c_func($proto, $body);
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
