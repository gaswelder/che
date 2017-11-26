<?php

// <typedef>: "typedef" <type> <form> ";"
parser::extend('typedef', function(parser $p) {
	$p->s->expect('typedef');
	$type = $p->read('type');
	$form = $p->read('form');
	$p->s->expect(';');
	return new c_typedef($type, $form);
});

// <macro>: "#include" <string> | "#define" <name> <string>
// "#ifdef" <id> | "#ifndef" <id>
parser::extend('macro', function(parser $p) {
	$m = $p->s->get();

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
		$p->add_type($name);
		return new c_comment("#type $name");
	default:
		return $p->error("Unknown macro: $type");
	}
});

// <struct-def>: "pub"? "struct" <name> [<struct-fields>] ";"
parser::extend('struct-def', function(parser $p) {
	$s = $p->s;

	$pub = false;
	if ($s->peek()->type == 'pub') {
		$pub = true;
		$s->get();
	}

	$p->s->expect('struct');
	if ($s->peek()->type == 'word') {
		$name = $s->get()->content;
	}
	else {
		$name = '';
	}
	$def = new c_structdef($name);
	$def->pub = $pub;

	if ($s->peek()->type != '{') {
		return $def;
	}

	$s->get();
	while (!$s->ended() && $s->peek()->type != '}') {
		if ($s->peek()->type == 'union') {
			$u = $p->read('union');
			$type = new c_type(array($u));
			$form = $p->read('form');

			$list = new c_varlist($type);
			$list->add($form);
		}
		else {
			$list = $p->read('varlist');
		}
		$def->add($list);
		$p->s->expect(';');
	}
	$p->s->expect('}');
	$p->s->expect(';');

	return $def;
});

parser::extend('varlist', function(parser $p) {
	$s = $p->s;

	$type = $p->read('type');
	$list = new c_varlist($type);

	$f = $p->read('form');
	$e = null;
	if ($s->peek()->type == '=') {
		$s->get();
		$e = $p->read('expr');
	}
	$list->add($f, $e);

	while ($s->peek()->type == ',') {
		$comma = $s->get();

		if (!$p->form_follows()) {
			$s->unget($comma);
			break;
		}
		$f = $p->read('form');
		$e = null;
		if ($s->peek()->type == '=') {
			$s->get();
			$e = $p->read('expr');
		}
		$list->add($f, $e);
	}
	return $list;
});

parser::extend('if', function(parser $p) {
	$p->s->expect('if');
	$p->s->expect('(');
	$expr = $p->read('expr');
	$p->s->expect(')');

	$body = $p->read('body-or-part');

	$t = $p->s->peek();
	if ($t->type == 'else') {
		$p->s->get();
		$else = $p->read('body-or-part');
	}
	else {
		$else = null;
	}

	return new c_if($expr, $body, $else);
});

// <expr>: <atom> [<op> <atom>]...
parser::extend('expr', function (parser $p) {
	$s = $p->s;

	$e = new c_expr();
	$e->add($p->read('atom'));

	while (!$s->ended() && $p->is_op($s->peek())) {
		$e->add($s->get()->type);
		$e->add($p->read('atom'));
	}

	return $e;
});

// <expr-atom>: <cast>? (
// 		<literal> / <sizeof> /
// 		<left-op>... ("(" <expr> ")" / <name>) <right-op>...
// )
parser::extend('atom', function (parser $parser) {
	$s = $parser->s;

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
	if ($s->peek()->type == 'sizeof') {
		$ops[] = array(
			'sizeof',
			$parser->read('sizeof')
		);
		return $ops;
	}

	// <left-op>...
	$L = array('&', '*', '++', '--', '!', '~');
	while (!$s->ended() && in_array($s->peek()->type, $L)) {
		$ops[] = array('op', $s->get()->type);
	}

	// "(" <expr> ")" / <name>
	if ($s->peek()->type == '(') {
		$parser->s->expect('(');
		$ops[] = array('expr', $parser->read('expr'));
		$parser->s->expect(')');
	}
	else {
		$t = $parser->s->expect('word');
		$ops[] = array('id', $t->content);
	}

	while (!$s->ended()) {
		$p = $s->peek();
		if ($p->type == '--' || $p->type == '++') {
			$ops[] = array(
				'op',
				$s->get()->type
			);
			continue;
		}

		if ($p->type == '[') {
			$s->get();
			$ops[] = array(
				'index',
				$parser->read('expr')
			);
			$parser->s->expect(']');
			continue;
		}

		if ($p->type == '.' || $p->type == '->') {
			$s->get();
			$ops[] = array('op', $p->type);
			$t = $s->get();
			if ($t->type != 'word') {
				return $parser->error("Id expected, got $t");
			}
			$ops[] = array('id', $t->content);
			continue;
		}

		if ($p->type == '(') {
			$s->get();
			$args = [];
			while (!$s->ended() && $s->peek()->type != ')') {
				$args[] = $parser->read('expr');
				if ($s->peek()->type == ',') {
					$s->get();
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
parser::extend('sizeof', function(parser $p) {
	$s = $p->s;
	$parts = [];

	$p->s->expect('sizeof');

	$brace = false;
	if ($s->peek()->type == '(') {
		$brace = true;
		$s->get();
	}

	if ($p->type_follows()) {
		$arg = $p->read('typeform');
	}
	else {
		$arg = $p->read('expr');
	}

	if ($brace) {
		$p->s->expect(')');
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
		$p = $parser->s->peek();
		$parser->s->unget($t);

		if ($p->type == '.') {
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
	$s = $parser->s;

	$struct = new c_struct_literal();

	$parser->s->expect('{');
	while (!$s->ended()) {
		$parser->s->expect('.');
		$id = $parser->s->expect('word')->content;
		$parser->s->expect('=');
		$val = $parser->read('literal');

		$struct->add($id, $val);

		if ($s->peek()->type == ',') {
			$s->get();
		}
		else {
			break;
		}
	}
	$parser->s->expect('}');
	return $struct;
});

parser::extend('array-literal', function(parser $parser) {
	$s = $parser->s;

	$elements = array();

	$parser->s->expect('{');
	while (!$s->ended() && $s->peek()->type != '}') {
		$elements[] = $parser->read('literal');
		if ($s->peek()->type == ',') {
			$s->get();
		}
	}
	$parser->s->expect('}');

	return new c_array($elements);
});

parser::extend('addr-literal', function(parser $parser) {
	$s = $parser->s;
	$str = '&';
	$parser->s->expect('&');
	$str .= $parser->s->expect('word')->content;
	return new c_literal($str);
});

parser::extend('typeform', function(parser $p) {
	$t = $p->read('type');
	$f = $p->read('form');
	return new c_typeform($t, $f);
});

// <defer>: "defer" <expr>
parser::extend('defer', function(parser $p) {
	$s = $p->s;
	$p->s->expect('defer');
	return new c_defer($p->read('expr'));
});

// <switch-case>: "case" <literal>|<id> ":" <body-part>...
parser::extend('switch-case', function(parser $parser) {
	$s = $parser->s;

	$parser->s->expect('case');

	// <id> | <literal>
	$p = $s->peek();
	if ($p->type == 'word') {
		$val = new c_literal($s->get()->content);
	}
	else {
		$val = $parser->read('literal');
	}

	// :
	$parser->s->expect(':');

	// <body-part>...
	$body = new c_body();
	while (!$s->ended()) {
		$t = $s->peek();
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

	$s = $parser->s;
	$cases = [];

	$parser->s->expect('{');
	while (!$s->ended() && $s->peek()->type != '}') {
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
	$s = $parser->s;
	$body = new c_body();

	$parser->s->expect('{');
	while (!$s->ended() && $s->peek()->type != '}') {
		$part = $parser->read('body-part');
		$body->add($part);
	}
	$parser->s->expect('}');
	return $body;
});

// <body-part>: (comment | <obj-def> | <construct>
// 	| (<expr> ";") | (<defer> ";") | <body> )...
parser::extend('body-part', function(parser $parser) {
	$s = $parser->s;
	$t = $s->peek();

	// comment?
	if ($t->type == 'comment') {
		$s->get();
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

parser::extend('body-or-part', function(parser $p) {
	if ($p->s->peek()->type == '{') {
		return $p->read('body');
	}

	$b = new c_body();
	$b->add($p->read('body-part'));
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
	$s = $parser->s;
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
	while (!$s->ended()) {
		$t = $s->get();

		if ($t->type == '(') {
			$mod *= 10;
			continue;
		}

		if ($t->type == '*') {
			$left[] = array('*', 1 * $mod);
			continue;
		}

		$s->unget($t);
		break;
	}

	// <name>?
	if ($s->peek()->type == 'word') {
		$name = $s->get()->content;
	}
	else {
		$name = '';
	}

	// <right-mod>
	while (!$s->ended()) {
		$t = $parser->s->get();
		$ch = $t->type;

		if ($ch == ')' && $mod > 1) {
			$mod /= 10;
			continue;
		}

		// <call-signature>?
		if ($ch == '(') {
			$s->unget($t);
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
			$s->unget($t);
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

parser::extend('form', function(parser $p) {
	$f = $p->read('obj-der');
	return new c_form($f['name'], $f['ops']);
});

parser::extend('type', function(parser $parser) {
	$s = $parser->s;

	$mods = array();

	if ($s->peek()->type == 'const') {
		$mods[] = 'const';
		$s->get();
	}

	$type = array();

	// "struct" <struct-fields>?
	if ($s->peek()->type == 'struct') {
		$type[] = $parser->read('struct-def');
		return new c_type(array_merge($mods, $type));
	}

	$t = $s->peek();
	while ($t->type == 'word' && $parser->is_typename($t->content)) {
		$type[] = $t->content;
		$s->get();
		$t = $s->peek();
	}

	if (empty($type)) {
		if ($s->peek()->type == 'word') {
			$id = $s->peek()->content;
			return $parser->error("Unknown type name: $id");
		}
		return $parser->error("Type name expected, got ".$s->peek());
	}

	return new c_type(array_merge($mods, $type));
});

parser::extend('union', function(parser $parser) {
	$s = $parser->s;
	$parser->s->expect('union');
	$parser->s->expect('{');

	$u = new c_union();

	while (!$s->ended() && $s->peek()->type != '}') {
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
	$s = $parser->s;

	$pub = false;
	if ($s->peek()->type == 'pub') {
		$pub = true;
		$s->get();
	}

	$parser->s->expect('enum');
	$parser->s->expect('{');

	$enum = new c_enum();
	$enum->pub = $pub;
	while (1) {
		$id = $parser->s->expect('word')->content;
		$val = null;
		if ($s->peek()->type == '=') {
			$s->get();
			$val = $parser->read('literal');
		}
		$enum->add($id, $val);
		if ($s->peek()->type == ',') {
			$s->get();
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
	$s = $parser->s;

	$pub = false;

	if ($s->peek()->type == 'pub') {
		$s->get();
		$pub = true;
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
		if ($s->peek()->type == '=') {
			$s->get();
			$expr = $parser->read('expr');
		}
		$parser->s->expect(';');
		$list = new c_varlist($type);
		$list->add($form, $expr);
		return $list;
	}

	$args = array_shift($form->ops);
	$proto = new c_prototype($type, $form, $args);
	$proto->pub = $pub;

	$t = $s->get();
	if ($t->type == ';') {
		return $proto;
	}

	if ($t->type == '{') {
		$s->unget($t);
		$body = $parser->read('body');
		return new c_func($proto, $body);
	}

	return $parser->error("Unexpected $t");
});

parser::extend('root', function(parser $parser) {
	$s = $parser->s;
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
		$path = $parser->s->get();
		$dir = dirname(realpath($parser->path));

		return new c_import($path->content, $dir);
	}

	/*
		* If 'pub' follows, look at the token after that.
		*/
	if ($t->type == 'pub') {
		$t1 = $s->peek(1);
		$t2 = $s->peek(2);
		$t3 = $s->peek(3);
	}
	else {
		$t1 = $s->peek(0);
		$t2 = $s->peek(1);
		$t3 = $s->peek(2);
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