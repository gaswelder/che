<?php

parser::extend('root', function(parser $parser) {
	$t = $parser->s->peek();
	
	// <macro>?
	if ($t->type == 'macro') {
		return $parser->read('macro');
	}

	// <typedef>?
	if ($t->type == 'typedef') {
		$def = $parser->read('typedef');
		$parser->add_type($def->form->name);
		return $def;
	}

	// comment?
	if ($t->type == 'comment') {
		$parser->s->get();
		return new c_comment($t->content);
	}

	if ($t->type == 'import') {
		$parser->s->get();
		$parserath = $parser->s->get();
		$dir = dirname(realpath($parser->path));

		return new c_import($parserath->content, $dir);
	}

	/*
		* If 'pub' follows, look at the token after that.
		*/
	if ($t->type == 'pub') {
		$t1 = $parser->s->peek(1);
		$t2 = $parser->s->peek(2);
		$t3 = $parser->s->peek(3);
	}
	else {
		$t1 = $parser->s->peek(0);
		$t2 = $parser->s->peek(1);
		$t3 = $parser->s->peek(2);
	}

	// <enum-def>?
	if ($t1->type == 'enum') {
		return $parser->read('enum-def');
	}

	// <struct-def>?
	if ($t1->type == 'struct') {
		if ($t2->type == 'word' && $t3->type == '{') {
			$def = $parser->read('struct-def');
			return $def;
		}
	}

	// <obj-def>
	return $parser->read('object-def');
});


// <typedef>: "typedef" <type> <form> ";"
parser::extend('typedef', function(parser $parser) {
	$parser->s->expect('typedef');
	$type = $parser->read('type');
	$form = $parser->read('form');
	$parser->s->expect(';');
	return new c_typedef($type, $form);
});

// <macro>: "#include" <string> | "#define" <name> <string>
// "#ifdef" <id> | "#ifndef" <id>
parser::extend('macro', function(parser $parser) {
	$m = $parser->s->get();

	$type = strtok($m->content, " \t");
	switch ($type) {
	case '#include':
		$parserath = trim(strtok("\n"));
		return new c_include($parserath);
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

// <struct-def>: "pub"? "struct" <name> [<struct-fields>] ";"
parser::extend('struct-def', function(parser $parser) {
	$parserub = false;
	if ($parser->s->peek()->type == 'pub') {
		$parserub = true;
		$parser->s->get();
	}

	$parser->s->expect('struct');
	if ($parser->s->peek()->type == 'word') {
		$name = $parser->s->get()->content;
	}
	else {
		$name = '';
	}
	$def = new c_structdef($name);
	$def->pub = $parserub;

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

parser::extend('if', function(parser $parser) {
	$parser->s->expect('if');
	$parser->s->expect('(');
	$expr = $parser->read('expr');
	$parser->s->expect(')');

	$body = $parser->read('body-or-part');

	$t = $parser->s->peek();
	if ($t->type == 'else') {
		$parser->s->get();
		$else = $parser->read('body-or-part');
	}
	else {
		$else = null;
	}

	return new c_if($expr, $body, $else);
});

// <expr>: <atom> [<op> <atom>]...
parser::extend('expr', function (parser $parser) {
	$e = new c_expr();
	$e->add($parser->read('atom'));

	while (!$parser->s->ended() && $parser->is_op($parser->s->peek())) {
		$e->add($parser->s->get()->type);
		$e->add($parser->read('atom'));
	}

	return $e;
});

// <expr-atom>: <cast>? (
// 		<literal> / <sizeof> /
// 		<left-op>... ("(" <expr> ")" / <name>) <right-op>...
// )
parser::extend('atom', function (parser $parser) {
	$ops = array();

	// <cast>?
	if ($parser->cast_follows()) {
		$parser->s->expect('(');
		$tf = $parser->read('typeform');
		$parser->s->expect(')');
		$ops[] = array('cast', $tf);
	}

	// <literal>?
	if ($parser->literal_follows()) {
		$ops[] = array(
			'literal',
			$parser->read('literal')
		);
		return $ops;
	}

	// <sizeof>?
	if ($parser->s->peek()->type == 'sizeof') {
		$ops[] = array(
			'sizeof',
			$parser->read('sizeof')
		);
		return $ops;
	}

	// <left-op>...
	$L = array('&', '*', '++', '--', '!', '~');
	while (!$parser->s->ended() && in_array($parser->s->peek()->type, $L)) {
		$ops[] = array('op', $parser->s->get()->type);
	}

	// "(" <expr> ")" / <name>
	if ($parser->s->peek()->type == '(') {
		$parser->s->expect('(');
		$ops[] = array('expr', $parser->read('expr'));
		$parser->s->expect(')');
	}
	else {
		$t = $parser->s->expect('word');
		$ops[] = array('id', $t->content);
	}

	while (!$parser->s->ended()) {
		$peek = $parser->s->peek();
		if ($peek->type == '--' || $peek->type == '++') {
			$ops[] = array(
				'op',
				$parser->s->get()->type
			);
			continue;
		}

		if ($peek->type == '[') {
			$parser->s->get();
			$ops[] = array(
				'index',
				$parser->read('expr')
			);
			$parser->s->expect(']');
			continue;
		}

		if ($peek->type == '.' || $peek->type == '->') {
			$parser->s->get();
			$ops[] = array('op', $peek->type);
			$t = $parser->s->get();
			if ($t->type != 'word') {
				return $parser->error("Id expected, got $t");
			}
			$ops[] = array('id', $t->content);
			continue;
		}

		if ($peek->type == '(') {
			$parser->s->get();
			$args = [];
			while (!$parser->s->ended() && $parser->s->peek()->type != ')') {
				$args[] = $parser->read('expr');
				if ($parser->s->peek()->type == ',') {
					$parser->s->get();
				}
				else {
					break;
				}
			}
			$parser->s->expect(')');
			$ops[] = array('call', $args);
		}

		break;
	}

	return $ops;
});

// <sizeof>: "sizeof" <sizeof-arg>
// <sizeof-arg>: ("(" (<expr> | <type>) ")") | <expr> | <type>
parser::extend('sizeof', function(parser $parser) {
	$parserarts = [];

	$parser->s->expect('sizeof');

	$brace = false;
	if ($parser->s->peek()->type == '(') {
		$brace = true;
		$parser->s->get();
	}

	if ($parser->type_follows()) {
		$arg = $parser->read('typeform');
	}
	else {
		$arg = $parser->read('expr');
	}

	if ($brace) {
		$parser->s->expect(')');
	}

	return new c_sizeof($arg);
});

parser::extend('literal', function(parser $parser) {
	$t = $parser->s->get();

	if ($t->type == 'num') {
		return new c_number($t->content);
	}

	if ($t->type == 'string') {
		return new c_string($t->content);
	}

	if ($t->type == 'char') {
		return new c_char($t->content);
	}

	if ($t->type == '-') {
		$n = $parser->s->expect('num');
		return new c_number('-'.$n->content);
	}

	if ($t->type == '{') {
		$peek = $parser->s->peek();
		$parser->s->unget($t);

		if ($peek->type == '.') {
			return $parser->read('struct-literal');
		}
		else {
			return $parser->read('array-literal');
		}
	}

	if ($t->type == 'word') {
		return new c_literal($t->content);
	}

	if ($t->type == '&') {
		$parser->s->unget($t);
		return $parser->read('addr-literal');
	}

	return $parser->error("Unexpected $t");
});

// <struct-literal>: "{" "." <id> "=" <literal> [, ...] "}"
parser::extend('struct-literal', function(parser $parser) {
	$struct = new c_struct_literal();

	$parser->s->expect('{');
	while (!$parser->s->ended()) {
		$parser->s->expect('.');
		$id = $parser->s->expect('word')->content;
		$parser->s->expect('=');
		$val = $parser->read('literal');

		$struct->add($id, $val);

		if ($parser->s->peek()->type == ',') {
			$parser->s->get();
		}
		else {
			break;
		}
	}
	$parser->s->expect('}');
	return $struct;
});

parser::extend('array-literal', function(parser $parser) {
	$elements = array();

	$parser->s->expect('{');
	while (!$parser->s->ended() && $parser->s->peek()->type != '}') {
		$elements[] = $parser->read('literal');
		if ($parser->s->peek()->type == ',') {
			$parser->s->get();
		}
	}
	$parser->s->expect('}');

	return new c_array($elements);
});

parser::extend('addr-literal', function(parser $parser) {
	$str = '&';
	$parser->s->expect('&');
	$str .= $parser->s->expect('word')->content;
	return new c_literal($str);
});

parser::extend('typeform', function(parser $parser) {
	$t = $parser->read('type');
	$f = $parser->read('form');
	return new c_typeform($t, $f);
});

// <defer>: "defer" <expr>
parser::extend('defer', function(parser $parser) {
	$parser->s->expect('defer');
	return new c_defer($parser->read('expr'));
});

// <switch-case>: "case" <literal>|<id> ":" <body-part>...
parser::extend('switch-case', function(parser $parser) {
	$parser->s->expect('case');

	// <id> | <literal>
	$peek = $parser->s->peek();
	if ($peek->type == 'word') {
		$val = new c_literal($parser->s->get()->content);
	}
	else {
		$val = $parser->read('literal');
	}

	// :
	$parser->s->expect(':');

	// <body-part>...
	$body = new c_body();
	while (!$parser->s->ended()) {
		$t = $parser->s->peek();
		if ($t->type == 'case') {
			break;
		}
		if ($t->type == '}') {
			break;
		}
		$body->add($parser->read('body-part'));
	}
	return array($val, $body);
});

// <switch>: "switch" "(" <expr> ")" "{" <switch-case>... "}"
parser::extend('switch', function(parser $parser) {
	$parser->s->expect('switch');
	$parser->s->expect('(');
	$cond = $parser->read('expr');
	$parser->s->expect(')');

	$cases = [];

	$parser->s->expect('{');
	while (!$parser->s->ended() && $parser->s->peek()->type != '}') {
		$cases[] = $parser->read('switch-case');
	}
	$parser->s->expect('}');

	return new c_switch($cond, $cases);
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

parser::extend('while', function(parser $parser) {
	$parser->s->expect('while');
	$parser->s->expect('(');
	$cond = $parser->read('expr');
	$parser->s->expect(')');

	$body = $parser->read('body');

	return new c_while($cond, $body);
});

parser::extend('for', function(parser $parser) {
	$parser->s->expect('for');
	$parser->s->expect('(');

	if ($parser->type_follows()) {
		$init = $parser->read('varlist');
	}
	else {
		$init = $parser->read('expr');
	}
	$parser->s->expect(';');
	$cond = $parser->read('expr');
	$parser->s->expect(';');
	$act = $parser->read('expr');
	$parser->s->expect(')');

	$body = $parser->read('body');

	return new c_for($init, $cond, $act, $body);
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

parser::extend('form', function(parser $parser) {
	$f = $parser->read('obj-der');
	return new c_form($f['name'], $f['ops']);
});

parser::extend('type', function(parser $parser) {
	$mods = array();

	if ($parser->s->peek()->type == 'const') {
		$mods[] = 'const';
		$parser->s->get();
	}

	$type = array();

	// "struct" <struct-fields>?
	if ($parser->s->peek()->type == 'struct') {
		$type[] = $parser->read('struct-def');
		return new c_type(array_merge($mods, $type));
	}

	$t = $parser->s->peek();
	while ($t->type == 'word' && $parser->is_typename($t->content)) {
		$type[] = $t->content;
		$parser->s->get();
		$t = $parser->s->peek();
	}

	if (empty($type)) {
		if ($parser->s->peek()->type == 'word') {
			$id = $parser->s->peek()->content;
			return $parser->error("Unknown type name: $id");
		}
		return $parser->error("Type name expected, got ".$parser->s->peek());
	}

	return new c_type(array_merge($mods, $type));
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

// <enum-def>: "pub"? "enum" "{" <id> ["=" <literal>] [,]... "}" ";"
parser::extend('enum-def', function(parser $parser) {
	$parserub = false;
	if ($parser->s->peek()->type == 'pub') {
		$parserub = true;
		$parser->s->get();
	}

	$parser->s->expect('enum');
	$parser->s->expect('{');

	$enum = new c_enum();
	$enum->pub = $parserub;
	while (1) {
		$id = $parser->s->expect('word')->content;
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
	$parser->s->expect('}');
	$parser->s->expect(';');
	return $enum;
});

// <object-def>:
// "pub"? <type> <form> [= <expr>] ";"
// or <stor> <type> <form> ";"
// or <stor> <type> <func> ";"
// or <stor> <type> <func> <body>
parser::extend('object-def', function(parser $parser) {
	$parserub = false;

	if ($parser->s->peek()->type == 'pub') {
		$parser->s->get();
		$parserub = true;
	}

	$type = $parser->read('type');
	$form = $parser->read('form');

	/*
	 * If not a function, return as a varlist.
	 */
	if (empty($form->ops) || !($form->ops[0] instanceof c_formal_args)) {
		if ($parserub) {
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
	$parserroto->pub = $parserub;

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
