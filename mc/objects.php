<?php

class c_element
{
	function __toString()
	{
		$t = get_class($this);
		$s = json_encode($this, JSON_PRETTY_PRINT);
		return "$t$s";
	}
}

class c_import extends c_element
{
	public $path;
	public $dir;

	public function __construct($path, $dir)
	{
		$this->path = $path;
		$this->dir = $dir;
	}

	function format()
	{
		return "import $this->path";
	}

	static function parse(parser $parser) {
		list ($path) = $parser->seq('import', '$literal-string');
		$dir = dirname(realpath($parser->path));
		return new c_import($path->content, $dir);
	}
}

class c_macro extends c_element
{
	public $type;
	public $arg = null;
	function __construct($type, $arg = null)
	{
		$this->type = $type;
		$this->arg = $arg;
	}

	function format()
	{
		$s = '#'.$this->type;
		if ($this->arg !== null) {
			$s .= " $this->arg";
		}
		return $s;
	}
}

class c_define extends c_element
{
	public $name;
	public $value;
	function __construct($name, $value)
	{
		$this->name = $name;
		$this->value = $value;
	}

	function format()
	{
		return "#define $this->name $this->value\n";
	}
}

/*
 * A #link directive
 */
class c_link
{
	public $name;
	function __construct($name)
	{
		$this->name = $name;
	}

	function format()
	{
		return "";
	}
}

class c_include extends c_element
{
	public $path;
	function __construct($path)
	{
		$this->path = $path;
	}

	function format()
	{
		return "#include $this->path\n";
	}
}

/*
 * Variable declarations list like "int a, *b, c()"
 */
class c_varlist
{
	public $stat = false;
	public $type;
	public $forms;
	public $values;

	function __construct(c_type $type)
	{
		$this->type = $type;
		$this->forms = array();
	}

	function typenames()
	{
		// return array_merge($this->type->typenames(), $this->)
		// self::type($l->type, $headers);
		// foreach ($l->values as $e) {
		// 	self::expr($e, $headers);
		// }
	}

	function add($var) {
		if ($var instanceof c_form) {
			$this->forms[] = $var;
			$this->values[] = null;
		} else {
			// assignment
			$this->forms[] = $var[0];
			$this->values[] = $var[1];
		}
	}

	function format()
	{
		$s = $this->type->format();
		foreach ($this->forms as $i => $f) {
			if ($i > 0) {
				$s .= ', ';
			}
			else {
				$s .= ' ';
			}
			$s .= $f->format();
			if ($this->values[$i] !== null) {
				$s .= ' = '.$this->values[$i]->format();
			}
		}

		if ($this->stat) {
			$s = 'static '.$s;
		}
		return $s;
	}
}

/*
 * Form, an object description like *foo[42] or myfunc(int, int).
 */
class c_form
{
	public $name;
	public $ops;

	function __construct($name, $ops)
	{
		$this->name = $name;
		$this->ops = $ops;
	}

	function format()
	{
		$s = $this->name;
		$n = count($this->ops);
		$i = 0;
		while ($i < $n) {
			$mod = $this->ops[$i++];

			if ($mod == '*') {
				$s = $mod.$s;
				continue;
			}

			if (is_string($mod) && strlen($mod) > 1 && $mod[0] == '[') {
				$s = $s.$mod;
				continue;
			}

			if ($mod instanceof c_formal_args) {
				$s .= $mod->format();
				continue;
			}

			if ($mod == 'struct') {
				$mod = $this->type[$i++];

				if (is_string($mod)) {
					$s = "struct $mod $s";
					continue;
				}
			}

			if (is_string($mod)) {
				$s = "$mod $s";
				continue;
			}

			echo '-----objects.php------------', "\n";
			echo "form $this->name\n";
			var_dump($mod);
			echo '-----------------', "\n";
			var_dump($this->type);
			echo '-----------------', "\n";
			exit;
		}

		return $s;
	}

	static function parse(parser $parser)
	{
		$f = $parser->read('obj-der');
		return new c_form($f['name'], $f['ops']);
	}
}

/*
 * Type, a list of type modifiers like 'int' or 'struct {...}'.
 */
class c_type
{
	public $l;
	function __construct($list)
	{
		$this->l = $list;
	}

	function format()
	{
		$s = array();
		foreach ($this->l as $t) {
			if (!is_string($t)) $t = $t->format();
			$s[] = $t;
		}
		return implode(' ', $s);
	}

	function typenames()
	{
		$names = [];
		foreach ($this->l as $t) {
			if (is_string($t)) {
				$names[] = $t;
			} else {
				$names[] = $t->format();
			}
		}
		return $names;
	}
}

/*
 * A combination of type and form, like in "(struct foo *[])".
 */
class c_typeform
{
	public $type;
	public $form;
	function __construct($type, $form)
	{
		$this->type = $type;
		$this->form = $form;
	}

	function format()
	{
		return $this->type->format().' '.$this->form->format();
	}

	static function parse(parser $parser)
	{
		$t = $parser->read('type');
		$f = $parser->read('form');
		return new c_typeform($t, $f);
	}
}

class c_typedef extends c_element
{
	public $form;
	public $type;

	function __construct(c_type $type, c_form $form)
	{
		if (!$form->name) {
			trigger_error("typedef: empty type name");
		}
		$this->type = $type;
		$this->form = $form;
	}

	function format()
	{
		return sprintf("typedef %s %s", $this->type->format(), $this->form->format());
	}

	function typenames()
	{
		return $this->type->typenames();
	}

	static function parse(parser $parser) {
		list($type, $form) = $parser->seq('typedef', '$type', '$form', ';');
		$parser->add_type($form->name);
		return new c_typedef($type, $form);
	}
}

class c_enum extends c_element
{
	public $pub = false;
	public $values = array();

	function add($name, $val)
	{
		$this->values[$name] = $val;
	}

	function format()
	{
		$s = "enum {\n";
		$n = count($this->values);
		$i = 0;
		foreach ($this->values as $name => $val) {
			$s .= "\t$name";
			if ($val !== null) {
				$s .= " = ".$val->format();
			}
			$i++;
			if ($i < $n) {
				$s .= ",";
			}
			$s .= "\n";
		}
		$s .= "};\n";
		return $s;
	}

	static function parse(parser $parser)
	{
		$pub = $parser->maybe('pub');
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
	}
}

class c_prototype
{
	public $pub = false;
	public $type;
	public $form;
	public $args;

	function __construct(c_type $type, c_form $form, c_formal_args $args)
	{
		assert($form->name != "");
		$this->type = $type;
		$this->form = $form;
		$this->args = $args;
	}

	function format()
	{
		$s = sprintf("%s %s%s;", $this->type->format(), $this->form->format(),
			$this->args->format());
		if (!$this->pub && $this->form->name != 'main') {
			$s = 'static '.$s;
		}
		return $s;
	}
}

/*
 * Formal arguments list for a function prototype, like '(int a, int *b)'.
 */
class c_formal_args
{
	public $groups = [];
	public $more = false; // ellipsis

	function add(c_formal_argsgroup $g)
	{
		$this->groups[] = $g;
	}

	function format()
	{
		$list = [];
		foreach ($this->groups as $g) {
			$list[] = $g->format();
		}
		if ($this->more) {
			$list[] = '...';
		}
		return '('.implode(', ', $list).')';
	}
}

class c_formal_argsgroup extends c_element
{
	public $type;
	public $forms = [];

	function format() {
		$list = [];
		foreach ($this->forms as $form) {
			$list[] = $this->type->format() . ' ' . $form->format();
		}
		return implode(', ', $list);
	}
}

class c_struct_identifier extends c_element
{
	public $name;

	function format() {
		if ($this->name) {
			return 'struct '.$this->name->format();
		}
		return 'struct';
	}

	static function parse(parser $parser) {
		list ($id) = $parser->seq('struct', '$identifier');
		$sid = new c_struct_identifier();
		$sid->name = $id;
		return $sid;
	}
}

class c_structdef extends c_element
{
	public $pub;
	public $name;
	public $fields = array();

	function __construct($name = null, $lists = [])
	{
		if (!$name) $name = new c_struct_identifier();
		$this->name = $name;
		foreach ($lists as $list) {
			$this->add($list);
		}
	}

	function add(c_varlist $list)
	{
		$this->fields[] = $list;
	}

	function format()
	{
		$s = $this->name->format();
		if (empty($this->fields)) {
			return $s;
		}

		$s .= " {\n";
		foreach ($this->fields as $list) {
			foreach ($list->forms as $f) {
				$s .= sprintf("\t%s %s;\n", $list->type->format(), $f->format());
			}
		}
		$s .= "}";
		return $s;
	}

	static function parse(parser $parser)
	{
		$pub = $parser->maybe('pub');
		list ($id) = $parser->seq('$struct-identifier', '{');
		$lists = $parser->many('struct-def-element');
		$parser->seq('}', ';');
	
		$def = new c_structdef($id, $lists);
		$def->pub = $pub;
		return $def;
	}
}

class c_union
{
	public $fields = array();
	// c_varlist[]
	function add(c_varlist $list)
	{
		$this->fields[] = $list;
	}

	function format()
	{
		$s = "union {\n";
		foreach ($this->fields as $list) {
			$s .= "\t".$list->format().";\n";
		}
		$s .= "}\n";
		return $s;
	}
}

class c_literal extends c_element
{
	public $content;
	function __construct($s)
	{
		$this->content = $s;
	}

	function format()
	{
		return $this->content;
	}
}

class c_string extends c_literal
{
	function format()
	{
		return '"'.$this->content.'"';
	}
}

class c_identifier extends c_literal {
	static function parse(parser $parser) {
		$name = $parser->expect('word')->content;
		if ($parser->is_typename($name)) {
			throw new ParseException("typename, not id");
		}
		return new c_identifier($name);
	}
}

class c_char extends c_literal
{
	function format()
	{
		return "'".$this->content."'";
	}
}

;

class c_number extends c_literal
{
	function format()
	{
		return $this->content;
	}

	static function parse(parser $parser)
	{
		// Optional minus sign.
		$s = '';
		try {
			$parser->expect('-');
			$s = '-';
		} catch (ParseException $e) {
			//
		}
		// The number token.
		$s .= $parser->expect('num')->content;
		return new c_number($s);
	}
}

class c_array extends c_literal
{
	function format()
	{
		$i = 0;
		$s = '{';
		foreach ($this->content as $e) {
			if ($i > 0) {
				$s .= ', ';
			}
			$i++;

			$s .= $e->format();
		}
		$s .= '}';
		return $s;
	}

	static function parse(parser $parser)
	{
		$parser->expect('{');			
		$elements = [];
		while (1) {
			try {
				$elements[] = $parser->read('array-literal-element');
			} catch (ParseException $e) {
				break;
			}
			try {
				$parser->expect(',');
			} catch (ParseException $e) {
				break;
			}
		}
		$parser->expect('}');
		return new c_array($elements);
	}
}

class c_designated_array_element extends c_element
{
	public $index;
	public $value;

	function format() {
		return "[$this->index] = " . $this->value->format();
	}

	static function parse(parser $parser)
	{
		$item = new c_designated_array_element;
		$parser->expect('[');
		try {
			$item->index = $parser->expect('word')->content;
		} catch (ParseException $e) {
			$item->index = $parser->expect('number')->content;
		}
		$parser->expect(']');
		$parser->expect('=');
		$item->value = $parser->read('constant-expression');
		return $item;
	}
}

class c_struct_literal extends c_element
{
	private $elements = [];

	function add($element)
	{
		$this->elements[] = $element;
	}

	function format()
	{
		$s = "{\n";
		foreach ($this->elements as $e) {
			$s .= "\t" . $e->format() . ",\n";
		}
		$s .= "}\n";
		return $s;
	}

	static function parse(parser $parser)
	{
		$struct = new c_struct_literal();
		$parser->expect('{');
		while (1) {
			try {
				$struct->add($parser->read('struct-literal-element'));
			} catch (ParseException $e) {
				break;
			}
			try {
				$parser->expect(',');
			} catch (ParseException $e) {
				break;
			}
		}
		$parser->expect('}');
		return $struct;
	}
}

class c_struct_literal_element extends c_element
{
	private $id;
	private $value;

	function __construct($id, $value)
	{
		$this->id = $id;
		$this->value = $value;
	}

	function format()
	{
		return sprintf('.%s = %s', $this->id->format(), $this->value->format());
	}
}

class c_sizeof extends c_element
{
	private $arg;

	function __construct($arg)
	{
		$this->arg = $arg;
	}

	function format()
	{
		$s = 'sizeof (';
		$s .= $this->arg->format();
		$s .= ')';
		return $s;
	}

	static function parse(parser $parser)
	{
		$parser->expect('sizeof');
		try {
			list ($arg) = $parser->seq('(', '$sizeof-contents', ')');
		} catch (ParseException $e) {
			list ($arg) = $parser->seq('$sizeof-contents');
		}
		return new c_sizeof($arg);
	}
}

class c_expr_atom extends c_element
{
	public $a;

	function __construct($a) {
		$this->a = $a;
	}

	function format()
	{
		$s = '';
		foreach ($this->a as $i) {
			switch ($i[0]) {
				case 'id':
				$s .= $i[1]->format();
				break;
			case 'op':
				$s .= $i[1];
				break;
			case 'struct-access-dot':
				$s .= '.' . $i[1]->format();
				break;
			case 'struct-access-arrow':
				$s .= '->' . $i[1]->format();
				break;
			case 'call':
				$s .= '(';
				$n = 0;
				foreach ($i[1] as $expr){
					if ($n > 0)$s .= ', ';
					$s .= $expr->format();
					$n++;
				}
				$s .= ')';
				break;
			case 'literal':
				$s .= $i[1]->format();
				break;
			case 'index':
				$s .= '['.$i[1]->format().']';
				break;
			case 'sizeof':
				$s .= $i[1]->format();
				break;
			case 'cast':
				$s .= '(';
				$s .= $i[1]->format();
				$s .= ')';
				break;
			case 'expr':
				$s .= '('.$i[1]->format().')';
				break;
			default:
				var_dump($i);
				exit(1);
			}
		}
		return $s;
	}

	static function parse(parser $parser)
	{
		$ops = array();

		// <cast>?
		try {
			$ops[] = $parser->read('typecast');
		} catch (ParseException $e) {
			//
		}
	
		try {
			$ops[] = ['literal', $parser->read('literal')];
			return new c_expr_atom($ops);
		} catch (ParseException $e) {
			//
		}
	
		try {
			$ops[] = ['sizeof', $parser->read('sizeof')];
			return new c_expr_atom($ops);
		} catch (ParseException $e) {
			//
		}
	
		// <left-op>...
		while (1) {
			try {
				$ops[] = ['op', $parser->read('left-operator')];
			} catch (ParseException $e) {
				break;
			}
		}
	
		try {
			list ($expr) = $parser->seq('(', '$expr', ')');
			$ops[] = ['expr', $expr];
		} catch (ParseException $e) {
			$ops[] = ['id', $parser->read('identifier')];
		}
	
		while (1) {
			try {
				$ops[] = $parser->any([
					'right-operator',
					'index-op',
					'struct-access-op',
					'call-op'
				]);
				continue;
			} catch (ParseException $e) {
				break;
			}
		}
	
		return new c_expr_atom($ops);
	}
}

class c_expr extends c_element
{
	public $parts;

	function __construct($parts = array())
	{
		$this->parts = $parts;
	}

	function add($p)
	{
		assert($p != null);
		$this->parts[] = $p;
	}

	function format($tab = 0)
	{
		if (empty($this->parts)) {
			return '';
		}
		$p = str_repeat("\t", $tab);
		$n = count($this->parts);
		$i = 0;
		$s = '';
		if ($this->parts[0] == '-') {
			$s = '-';
			$i++;
		}
		while ($i < $n) {
			$a = $this->parts[$i];
			$s .= $a->format();
			$i++;
			if ($i < $n) {
				$s .= ' '.$this->parts[$i].' ';
				$i++;
			}
		}
		return $p.$s;
	}

	static function parse(parser $parser)
	{
		$expr = new c_expr();

		// An expression may start with a minus.
		try {
			$parser->expect('-');
			$expr->add('-');
		} catch (ParseException $e) {
			//
		}
	
		$expr->add($parser->read('atom'));
	
		while (!$parser->s->ended() && $parser->is_op($parser->s->peek())) {
			$expr->add($parser->read('operator'));
			$expr->add($parser->read('atom'));
		}
	
		return $expr;
	}
}

class c_defer
{
	public $expr;

	function __construct(c_expr $e)
	{
		$this->expr = $e;
	}

	function format()
	{
		return "defer ".$this->expr->format();
	}

	static function parse(parser $parser)
	{
		list($expr) = $parser->seq('defer', '$expr', ';');
		return new c_defer($expr);
	}
}

class c_return extends c_expr
{
	function format($tab = 0)
	{
		return str_repeat("\t", $tab).'return '.parent::format();
	}
}

class c_body
{
	public $parts = array();

	function add($p)
	{
		$this->parts[] = $p;
	}

	function format($tab = 0)
	{
		$pref = str_repeat("\t", $tab);
		$s = "{\n";
		foreach ($this->parts as $part) {
			$s .= $part->format($tab+1);
			if (!self::is_construct($part)) {
				$s .= ';';
			}
			$s .= "\n";
		}
		$s .= "$pref}\n";
		return $s;
	}

	static function is_construct($part)
	{
		$a = ['c_if', 'c_for', 'c_while', 'c_switch'];
		return in_array(get_class($part), $a);
	}

	static function parse(parser $parser)
	{
		$body = new c_body();
		
		$parser->expect('{');
		while (1) {
			try {
				$part = $parser->any([
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
				$body->add($part);
			} catch (ParseException $e) {
				break;
			}
		}
		$parser->expect('}');
		return $body;
	}
}

class c_comment extends c_literal
{
	function format()
	{
		return "/* $this->content */";
	}

	static function parse(parser $parser) {
		$t = $parser->expect('comment');
		return new c_comment($t->content);
	}
}

class c_if
{
	public $cond;
	// c_expr
	public $body;
	// c_body
	public $else;

	function __construct($cond, $body, $else)
	{
		$this->cond = $cond;
		$this->body = $body;
		$this->else = $else;
	}

	function format($tab = 0)
	{
		$pref = str_repeat("\t", $tab);
		$s = $pref.sprintf("if (%s) ", $this->cond->format());
		$s .= $this->body->format($tab);
		if ($this->else) {
			$s .= $pref."else ".$this->else->format($tab);
		}
		return $s;
	}
}

class c_switch
{
	public $cond;
	public $cases;

	function format($tab = 0)
	{
		$pref = str_repeat("\t", $tab);

		$s = $pref.sprintf("switch (%s) {\n", $this->cond->format());

		foreach ($this->cases as $case) {
			$s .= $case->format();
		}

		$s .= "}\n";
		return $s;
	}

	static function parse(parser $parser)
	{
		$sw = new c_switch();
		list ($cond) = $parser->seq('switch', '(', '$expr', ')', '{');
		
		$sw->cond = $cond;
		
		while (!$parser->s->ended() && $parser->s->peek()->type != '}') {
			$sw->cases[] = $parser->read('switch-case');
		}
		$parser->expect('}');
		return $sw;
	}
}

class c_switch_case extends c_element
{
	public $value;
	public $body;

	function format() {
		return sprintf("case %s:\n%s\nbreak;", $this->value->format(), $this->body->format());
	}

	static function parse(parser $parser)
	{
		$parser->expect('case');
		
		// <id> | <literal>
		$val = $parser->any(['identifier', 'literal']);
		$parser->expect(':');
	
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
		$case = new self();
		$case->value = $val;
		$case->body = $body;
		return $case;
	}
}

class c_for
{
	public $init;
	// c_expr
	public $cond;
	// c_expr;
	public $act;
	public $body;
	function __construct($i, $c, $a, $b)
	{
		$this->init = $i;
		$this->cond = $c;
		$this->act = $a;
		$this->body = $b;
	}

	function format($tab = 0)
	{
		$p = str_repeat("\t", $tab);
		$s = $p.sprintf("for (%s; %s; %s) ", $this->init->format(), $this->cond->format(),
			$this->act->format());
		$s .= $this->body->format($tab);
		return $s;
	}

	static function parse(parser $parser)
	{
		$parser->seq('for', '(');
		$init = $parser->any(['varlist', 'expr']);
		list ($cond, $act, $body) = $parser->seq(';', '$expr', ';', '$expr', ')', '$body');
		return new c_for($init, $cond, $act, $body);
	}
}

class c_while
{
	public $cond;
	public $body;
	function __construct($cond, $body)
	{
		$this->cond = $cond;
		$this->body = $body;
	}

	function format($tab = 0)
	{
		$p = str_repeat("\t", $tab);
		$s = $p.sprintf("while (%s) ", $this->cond->format());
		$s .= $this->body->format($tab);
		return $s;
	}

	static function parse(parser $parser)
	{
		list ($cond, $body) = $parser->seq('while', '(', '$expr', ')', '$body');
		return new c_while($cond, $body);
	}
}

class c_func extends c_element
{
	public $proto;
	public $body;

	function __construct($proto, $body)
	{
		$this->proto = $proto;
		$this->body = $body;
	}

	function format($tab = 0)
	{
		$s = $this->proto->format();
		$s = substr($s, 0, -1);
		// remove ';'
		$s .= ' ';
		$s .= $this->body->format();
		return $s;
	}

	static function parse(parser $parser) {
		$pub = $parser->maybe('pub');
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
	}
}
