<?php

parser::extend('obj-der', function(parser $parser) {
	try {
		list ($obj) = $parser->seq('*', '$obj-der');
		$obj['ops'][] = '*';
		return $obj;
	} catch (ParseException $e) {
		//
	}

	try {
		$id = $parser->read('identifier');
	} catch (ParseException $e) {
		$id = new c_identifier('');
	}
	
	$right = [];
	while (1) {
		try {
			$right[] = $parser->any(['call-signature', 'index-signature']);
		} catch (ParseException $e) {
			break;
		}
	}

	return [
		'name' => $id->format(),
		'ops' => $right
	];
});

parser::extend('index-signature', function(parser $parser) {
	$parser->expect('[');
	try {
		$expr = $parser->read('expr');
		$parser->expect(']');
		return '['.$expr->format().']';
	} catch (ParseException $e) {
		//
	}
	$parser->expect(']');
	return '[]';
});

parser::extend('call-signature', function(parser $parser) {
	$args = new c_formal_args();
	$empty = true;
	$parser->expect('(');
	while (1) {
		try {
			$l = $parser->read('varlist');
			$args->add($l);
			$empty = false;
		} catch (ParseException $e) {
			break;
		}

		try {
			$parser->expect(',');
		} catch (ParseException $e) {
			break;
		}
	}

	if (!$empty) {
		try {
			$parser->expect('...');
			$args->more = true;
		} catch (ParseException $e) {
			//
		}
	}

	$parser->expect(')');
	return $args;
});

parser::extend('typeform', function(parser $parser) {
	$t = $parser->read('type');
	$f = $parser->read('form');
	return new c_typeform($t, $f);
});

parser::extend('form', function(parser $parser) {
	$f = $parser->read('obj-der');
	return new c_form($f['name'], $f['ops']);
});

// Type is a description of a pure value that we get after
// applying all function calls, dereferencing and indexing.
// That is, "int *" is not a type, only "int" is. The "*" part
// is contained in the "form".
parser::extend('type', function(parser $parser) {
	$mods = array();
	$type = array();

	// Const?
	try {
		$parser->expect('const');
		$mods[] = 'const';
	} catch (ParseException $e) {
		//
	}

	// "struct {foo x; bar y; ...}"?
	try {
		$type[] = $parser->read('anonymous-struct');
		return new c_type(array_merge($mods, $type));
	} catch (ParseException $e) {
		//
	}

	// "struct foo"?
	try {
		$type[] = $parser->read('struct-typename');
		return new c_type(array_merge($mods, $type));
	} catch (ParseException $e) {
		//
	}

	$type[] = $parser->read('typename');
	return new c_type(array_merge($mods, $type));
});

// Typename is a pure type name, like "int" or "foo", if "foo"
// has been declared as a type.
// "unsigned int" or "typename" where "typename" is a declared type
parser::extend('typename', function(parser $parser) {
	$names = [];
	$names[] = $parser->expect('word')->content;
	if (!$parser->is_typename($names[0])) {
		throw new ParseException("Not a typename: $names[0]");
	}
	try {
		$names[] = $parser->read('typename');
	} catch (ParseException $e) {
		//
	}
	return implode(' ', $names);
});

// "struct foo"
parser::extend('struct-typename', function(parser $parser) {
	$parser->expect('struct');
	$name = $parser->expect('word')->content;
	return new c_structdef($name);
});

// "struct {foo x; bar y; ...}"
parser::extend('anonymous-struct', function(parser $parser) {
	$parser->expect('struct');
	$parser->expect('{');

	$def = new c_structdef('');

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
	$parser->expect('}');
	$parser->expect(';');

	return $def;
});

parser::extend('union', function(parser $parser) {
	$parser->s->expect('union');
	$parser->s->expect('{');

	$u = new c_union();

	while (!$parser->s->ended() && $parser->s->peek()->type != '}') {
		$type = $parser->read('type');
		$form = $parser->read('form');
		$list = new c_varlist($type);
		$list->add($form);
		$u->add($list);
		$parser->s->expect(';');
	}
	$parser->s->expect('}');
	return $u;
});
