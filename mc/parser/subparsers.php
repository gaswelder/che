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
		'object-def'
	]);
});

parser::extend('comment', function(parser $parser) {
	$t = $parser->expect('comment');
	return new c_comment($t->content);
});

parser::extend('import', function(parser $parser) {
	$t = $parser->expect('import');
	$path = $parser->s->get();
	$dir = dirname(realpath($parser->path));
	return new c_import($path->content, $dir);
});


// <typedef>: "typedef" <type> <form> ";"
parser::extend('typedef', function(parser $parser) {
	$parser->expect('typedef');
	$type = $parser->read('type');
	$form = $parser->read('form');
	$parser->expect(';');
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

	$parser->expect('struct');
	$name = $parser->expect('word')->content;
	$def = new c_structdef($name);
	$def->pub = $pub;

	if ($parser->s->peek()->type != '{') {
		return $def;
	}

	$parser->s->get();
	while (!$parser->s->ended() && $parser->s->peek()->type != '}') {
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
		$def->add($list);
		$parser->s->expect(';');
	}
	$parser->s->expect('}');
	$parser->s->expect(';');

	return $def;
});

parser::extend('varlist', function(parser $parser) {
	$type = $parser->read('type');
	$list = new c_varlist($type);

	$f = $parser->read('form');
	$e = null;
	if ($parser->s->peek()->type == '=') {
		$parser->s->get();
		$e = $parser->read('expr');
	}
	$list->add($f, $e);

	while ($parser->s->peek()->type == ',') {
		$comma = $parser->s->get();

		if (!$parser->form_follows()) {
			$parser->s->unget($comma);
			break;
		}
		$f = $parser->read('form');
		$e = null;
		if ($parser->s->peek()->type == '=') {
			$parser->s->get();
			$e = $parser->read('expr');
		}
		$list->add($f, $e);
	}
	return $list;
});

// <return>: "return" [<expr>] ";"
parser::extend('return', function(parser $parser) {
	$parser->s->expect('return');
	if ($parser->s->peek()->type != ';') {
		$expr = $parser->read('expr');
	}
	else {
		$expr = new c_expr();
	}
	$parser->s->expect(';');
	return new c_return($expr->parts);
});



// <body>: "{" <body-part>... "}"
parser::extend('body', function(parser $parser) {
	$body = new c_body();

	$parser->s->expect('{');
	while (!$parser->s->ended() && $parser->s->peek()->type != '}') {
		$parserart = $parser->read('body-part');
		$body->add($parserart);
	}
	$parser->s->expect('}');
	return $body;
});

// <body-part>: (comment | <obj-def> | <construct>
// 	| (<expr> ";") | (<defer> ";") | <body> )...
parser::extend('body-part', function(parser $parser) {
	$t = $parser->s->peek();

	// comment?
	if ($t->type == 'comment') {
		$parser->s->get();
		return new c_comment($t->content);
	}

	// <construct>?
	$constructs = array(
		'if',
		'for',
		'while',
		'switch',
		'return'
	);
	if (in_array($t->type, $constructs)) {
		return $parser->read($t->type);
	}

	// <body>?
	if ($t->type == '{') {
		return $parser->body();
	}

	// <varlist>?
	if ($parser->type_follows()) {
		$list = $parser->read('varlist');
		$parser->s->expect(';');
		return $list;
	}

	// <defer>?
	if ($t->type == 'defer') {
		$d = $parser->read('defer');
		$parser->s->expect(';');
		return $d;
	}

	// <expr>
	$expr = $parser->read('expr');
	$parser->s->expect(';');
	return $expr;
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

// <object-def>:
// "pub"? <type> <form> [= <expr>] ";"
// or <stor> <type> <form> ";"
// or <stor> <type> <func> ";"
// or <stor> <type> <func> <body>
parser::extend('object-def', function(parser $parser) {
	$pub = false;
	try {
		$parser->expect('pub');
		$pub = true;
	} catch (ParseException $e) {
		//
	}

	$type = $parser->read('type');
	$form = $parser->read('form');

	/*
	 * If not a function, return as a varlist.
	 */
	if (empty($form->ops) || !($form->ops[0] instanceof c_formal_args)) {
		if ($pub) {
			return $parser->error("varlist can't be declared 'pub'");
		}
		$expr = null;
		if ($parser->s->peek()->type == '=') {
			$parser->s->get();
			$expr = $parser->read('expr');
		}
		$parser->s->expect(';');
		$list = new c_varlist($type);
		$list->add($form, $expr);
		return $list;
	}

	$args = array_shift($form->ops);
	$parserroto = new c_prototype($type, $form, $args);
	$parserroto->pub = $pub;

	$t = $parser->s->get();
	if ($t->type == ';') {
		return $parserroto;
	}

	if ($t->type == '{') {
		$parser->s->unget($t);
		$body = $parser->read('body');
		return new c_func($parserroto, $body);
	}

	return $parser->error("Unexpected $t");
});
