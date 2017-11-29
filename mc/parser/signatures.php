<?php

// <der>: <left-mod>... <name> <right-mod>...
//	or	<left-mod>... "(" <der> ")" <right-mod>... <call-signature>
parser::extend('obj-der', function(parser $parser) {
	/*
	 * Priorities:
	 * 2: x[] x()
	 * 1: *x
	 */
	/*
	 * We treat braces as priority modifiers:
	 *         *((*(foo[]))(int, double))[]
	 *         *:1  *:100  foo  []:200  ():100  []:2
	 */

	$mod = 1;
	$left = array();
	$right = array();

	// <left-mod>...
	while (!$parser->s->ended()) {
		$t = $parser->s->get();

		if ($t->type == '(') {
			$mod *= 10;
			continue;
		}

		if ($t->type == '*') {
			$left[] = array('*', 1 * $mod);
			continue;
		}

		$parser->s->unget($t);
		break;
	}

	// <name>?
	if ($parser->s->peek()->type == 'word') {
		$name = $parser->s->get()->content;
	}
	else {
		$name = '';
	}

	// <right-mod>
	while (!$parser->s->ended()) {
		$t = $parser->s->get();
		$ch = $t->type;

		if ($ch == ')' && $mod > 1) {
			$mod /= 10;
			continue;
		}

		// <call-signature>?
		if ($ch == '(') {
			$parser->s->unget($t);
			$right[] = array(
				$parser->read('call-signature'),
				$mod * 2
			);
			continue;
		}

		// "[" <expr> "]"
		if ($ch == '[') {
			$conv = '[';
			if ($parser->s->peek()->type != ']') {
				$conv .= $parser->read('expr')->format();
			}
			$parser->s->expect(']');
			$conv .= ']';

			$right[] = array($conv, $mod * 2);
			continue;
		}

		if ($mod == 1) {
			$parser->s->unget($t);
			break;
		}

		return $parser->error("Unexpected $t");
	}

	/*
	 * Merge left and right modifiers into a single list.
	 */
	$left = array_reverse($left);
	$mods = array();
	while (!empty($left) && !empty($right)) {
		if ($right[0][1] > $left[0][1]) {
			$mods[] = array_shift($right)[0];
		}
		else {
			$mods[] = array_shift($left)[0];
		}
	}

	while (!empty($left)) {
		$mods[] = array_shift($left)[0];
	}
	while (!empty($right)) {
		$mods[] = array_shift($right)[0];
	}

	return array('name' => $name, 'ops' => $mods);
});

parser::extend('call-signature', function(parser $parser) {
	$args = new c_formal_args();

	$parser->s->expect('(');
	if ($parser->type_follows()) {
		while ($parser->type_follows()) {
			$l = $parser->read('varlist');
			$args->add($l);

			if ($parser->s->peek()->type != ',') {
				break;
			}

			$parser->s->get();
			if ($parser->s->peek()->type == '...') {
				$parser->s->get();
				$args->more = true;
				break;
			}
		}
	}
	$parser->s->expect(')');
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
