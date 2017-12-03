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

parser::extend('formal-argsgroup', function(parser $parser) {
	$group = new c_formal_argsgroup();
	$group->type = $parser->read('type');
	
	$forms = [$parser->read('form')];
	$group->forms = array_merge($forms, $parser->many('formal-argsgroup-cont'));
	return $group;
});

parser::extend('formal-argsgroup-cont', function(parser $parser) {
	list ($form) = $parser->seq(',', '$form');
	if ($form->format() == '') {
		throw new ParseException('empty form');
	}
	return $form;
});

parser::extend('call-signature', function(parser $parser) {
	$args = new c_formal_args();
	$parser->expect('(');
	while (1) {
		try {
			$g = $parser->read('formal-argsgroup');
			$args->add($g);
		} catch (ParseException $e) {
			break;
		}

		try {
			$parser->expect(',');
		} catch (ParseException $e) {
			break;
		}

		try {
			$parser->expect('...');
			$args->more = true;
			break;
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
	$parser->seq('struct', '{');
	$lists = $parser->many('struct-def-element');
	$parser->seq('}', ';');

	$def = new c_structdef('');
	foreach ($lists as $list) {
		$def->add($list);
	}
	return $def;
});

parser::extend('union', function(parser $parser) {
	$parser->seq('union', '{');
	$lists = $parser->many('union-def-element');
	$parser->expect('}');

	$u = new c_union();
	foreach ($lists as $list) {
		$u->add($list);
	}
	return $u;
});

parser::extend('union-def-element', function(parser $parser) {
	list ($type, $form) = $parser->seq('$type', '$form', ';');
	$list = new c_varlist($type);
	$list->add($form);
	return $list;
});
