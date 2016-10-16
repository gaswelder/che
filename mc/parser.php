<?php

// Second stage parser that returns CPOM objects

class parser
{
	private $s;

	private $type_names = array(
		'struct', 'enum', 'union',
		'void', 'char', 'short', 'int',
		'long', 'float', 'double', 'unsigned',
		'bool', 'va_list', 'FILE',
		'ptrdiff_t', 'size_t', 'wchar_t',
		'int8_t', 'int16_t', 'int32_t', 'int64_t',
		'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
		'clock_t', 'time_t',
		'fd_set', 'socklen_t', 'ssize_t'
	);

	private $path;

	function __construct( $path ) {
		$this->path = $path;
		$this->s = new mctok( $path );
	}

	function ended() {
		return $this->s->ended();
	}

	function error($msg) {
		$p = $this->s->peek();
		$pos = $p ? $p->pos : "EOF";
		$pos = $this->path . ':'.$pos;
		fwrite(STDERR, "$pos: $msg\n");
		fwrite(STDERR, $this->context()."\n");
		exit(1);
	}

	private function trace( $m ) {
		return;
		$s = $this->context();
		fwrite( STDERR, "--- $m\t|\t$s\n" );
	}

	function add_type($name) {
		//trace("New type: $name");
		$this->type_names[] = $name;
	}

	/*
	 * Reads next program object from the source.
	 */
	function get()
	{
		$s = $this->s;

		if( $s->ended() ) {
			return null;
		}

		$t = $this->s->peek();

		// <macro>?
		if( $t->type == 'macro' ) {
			return $this->read_macro();
		}

		// <typedef>?
		if( $t->type == 'typedef' ) {
			$def = $this->read_typedef();
			$this->add_type($def->form->name);
			return $def;
		}

		// <struct-def>?
		if( $t->type == 'struct' ) {
			if( $s->peek(1)->type == 'word' && $s->peek(2)->type == '{' ) {
				$def = $this->struct_def();
				return $def;
			}
		}

		// comment?
		if( $t->type == 'comment' ) {
			$this->s->get();
			return new c_comment( $t->content );
		}

		if( $t->type == 'import' )
		{
			$this->s->get();
			$path = $this->s->get();
			$dir = dirname(realpath($this->path));

			return new c_import($path->content, $dir);
		}

		/*
		 * If 'pub' follows, look at the token after that.
		 */
		if($t->type == 'pub') {
			$s->get();
			$next = $s->peek();
			$s->unget($t);
			$t = $next;
		}

		// <enum-def>?
		if( $t->type == 'enum' ) {
			return $this->read_enumdef();
		}

		// <obj-def>
		return $this->read_object();
	}

	//--

	// <macro>: "#include" <string> | "#define" <name> <string>
	// "#ifdef" <id> | "#ifndef" <id>
	private function read_macro()
	{
		$m = $this->s->get();

		$type = strtok( $m->content, " \t" );
		switch( $type )
		{
			case '#include':
				$path = trim( strtok( "\n" ) );
				return new c_include( $path );

			case '#define':
				$name = strtok( "\t " );
				$value = strtok( "\n" );
				return new c_define( $name, $value );

			case '#ifdef':
			case '#ifndef':
				$id = trim( strtok( "\n" ) );
				return new c_macro( $type, $id );

			case '#endif':
				return new c_macro( $type );

			default:
				return $this->error( "Unknown macro: $type" );
		}
	}

	// <typedef>: "typedef" <type> <form> ";"
	private function read_typedef()
	{
$this->trace( "read_typedef" );
		$this->expect( 'typedef' );
		$type = $this->type();
		$form = $this->form();
		$this->expect( ';' );
		return new c_typedef($type, $form);
	}

	// <struct-def>: "struct" <name> [<struct-fields>] ";"
	private function struct_def()
	{
$this->trace( "struct_def" );
		$s = $this->s;
		$this->expect( 'struct' );
		if($s->peek()->type == 'word') {
			$name = $s->get()->content;
		}
		else {
			$name = '';
		}
		$def = new c_structdef( $name );

		if($s->peek()->type != '{') {
			return $def;
		}

		$s->get();
		while(!$s->ended() && $s->peek()->type != '}')
		{
			if($s->peek()->type == 'union') {
				$u = $this->read_union();
				$type = new c_type(array($u));
				$form = $this->form();

				$list = new c_varlist($type);
				$list->add($form);
			}
			else {
				$list = $this->read_varlist();
			}
			$def->add($list);
			$this->expect(';');
		}
		$this->expect('}');
		$this->expect( ';' );

		return $def;
	}

	private function read_union()
	{
$this->trace( "read_union" );
		$s = $this->s;
		$this->expect( 'union' );
		$this->expect( '{' );

		$u = new c_union();

		while(!$s->ended() && $s->peek()->type != '}') {
			$type = $this->type();
			$form = $this->form();
			$list = new c_varlist($type);
			$list->add($form);
			$u->add($list);
			$this->expect( ';' );
		}
		$this->expect( '}' );
		return $u;
	}

	// <enum-def>: "pub"? "enum" "{" <id> ["=" <literal>] [,]... "}" ";"
	private function read_enumdef()
	{
		$s = $this->s;

		$pub = false;
		if($s->peek()->type == 'pub') {
			$pub = true;
			$s->get();
		}

		$this->expect( 'enum' );
		$this->expect( '{' );

		$enum = new c_enum();
		$enum->pub = $pub;
		while( 1 ) {
			$id = $this->expect( 'word' )->content;
			$val = null;
			if( $s->peek()->type == '=' ) {
				$s->get();
				$val = $this->literal();
			}
			$enum->add( $id, $val );
			if( $s->peek()->type == ',' ) {
				$s->get();
			} else {
				break;
			}
		}
		$this->expect( '}' );
		$this->expect( ';' );
		return $enum;
	}

	// <object-def>:
	// "pub"? <type> <form> [= <expr>] ";"
	// or <stor> <type> <form> ";"
	// or <stor> <type> <func> ";"
	// or <stor> <type> <func> <body>
	private function read_object()
	{
$this->trace( "read_object" );
		$s = $this->s;

		$pub = false;

		if($s->peek()->type == 'pub') {
			$s->get();
			$pub = true;
		}

		$type = $this->type();
		$form = $this->form();

		/*
		 * If not a function, return as a varlist.
		 */
		if(empty($form->ops) || !($form->ops[0] instanceof c_formal_args)) {
			if($pub) {
				return $this->error("varlist can't be declared 'pub'");
			}
			$expr = null;
			if($s->peek()->type == '=') {
				$s->get();
				$expr = $this->expr();
			}
			$this->expect(';');
			$list = new c_varlist($type);
			$list->add($form, $expr);
			return $list;
		}

		$args = array_shift($form->ops);
		$proto = new c_prototype( $type, $form, $args );
		$proto->pub = $pub;

		$t = $s->get();
		if( $t->type == ';' ) {
			return $proto;
		}

		if( $t->type == '{' ) {
			$s->unget( $t );
			$body = $this->read_body();
			return new c_func( $proto, $body );
		}

		return $this->error( "Unexpected $t" );
	}

	//--

	private function type()
	{
$this->trace( "type" );
		$s = $this->s;

		$mods = array();

		if($s->peek()->type == 'const') {
			$mods[] = 'const';
			$s->get();
		}

		$type = array();

		// "struct" <struct-fields>?
		if( $s->peek()->type == 'struct' ) {
			$type[] = $this->struct_def();
			return new c_type(array_merge($mods, $type));
		}

		$t = $s->peek();
		while( $t->type == 'word' && $this->is_typename( $t->content ) ) {
			$type[] = $t->content;
			$s->get();
			$t = $s->peek();
		}

		if(empty($type)) {
			if($s->peek()->type == 'word') {
				$id = $s->peek()->content;
				return $this->error("Unknown type name: $id");
			}
			return $this->error("Type name expected, got ".$s->peek());
		}

		return new c_type(array_merge($mods, $type));
	}

	private function form()
	{
		$f = $this->obj_der();
		return new c_form($f['name'], $f['ops']);
	}

	// <der>: <left-mod>... <name> <right-mod>...
	//	or	<left-mod>... "(" <der> ")" <right-mod>... <call-signature>
	private function obj_der()
	{
$this->trace( "obj_der" );
		$s = $this->s;
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
		while( !$s->ended() )
		{
			$t = $s->get();

			if( $t->type == '(' ) {
				$mod *= 10;
				continue;
			}

			if( $t->type == '*' ) {
				$left[] = array( '*', 1 * $mod );
				continue;
			}

			$s->unget( $t );
			break;
		}

		// <name>?
		if( $s->peek()->type == 'word' ) {
			$name = $s->get()->content;
		}
		else {
			$name = '';
		}

		// <right-mod>
		while( !$s->ended() )
		{
			$t = $this->s->get();
			$ch = $t->type;

			if( $ch == ')' && $mod > 1 ) {
				$mod /= 10;
				continue;
			}

			// <call-signature>?
			if( $ch == '(' ) {
				$s->unget( $t );
				$right[] = array( $this->call_signature(), $mod * 2 );
				continue;
			}

			// "[" <expr> "]"
			if( $ch == '[' )
			{
				$conv = '[';
				if($this->s->peek()->type != ']') {
					$conv .= $this->expr()->format();
				}
				$this->expect( ']' );
				$conv .= ']';

				$right[] = array( $conv, $mod * 2 );
				continue;
			}

			if( $mod == 1 ) {
				$s->unget( $t );
				break;
			}

			return $this->error( "Unexpected $t" );
		}

		/*
		 * Merge left and right modifiers into a single list.
		 */
		$left = array_reverse( $left );
		$mods = array();
		while( !empty( $left ) && !empty( $right ) ) {
			if( $right[0][1] > $left[0][1] ) {
				$mods[] = array_shift( $right )[0];
			} else {
				$mods[] = array_shift( $left )[0];
			}
		}

		while( !empty( $left ) ) {
			$mods[] = array_shift( $left )[0];
		}
		while( !empty( $right ) ) {
			$mods[] = array_shift( $right )[0];
		}

		return array( 'name' => $name, 'ops' => $mods );
	}

	private function call_signature()
	{
$this->trace("call_signature");
		$args = new c_formal_args();

		$this->expect('(');
		if($this->type_follows())
		{
			while($this->type_follows()) {
				$l = $this->read_varlist();
				$args->add($l);

				if($this->s->peek()->type != ',') {
					break;
				}

				$this->s->get();
				if($this->s->peek()->type == '...') {
					$this->s->get();
					$args->more = true;
					break;
				}
			}
		}
		$this->expect(')');
		return $args;
	}

	// <body>: "{" <body-part>... "}"
	private function read_body()
	{
$this->trace( "read_body" );
		$s = $this->s;
		$body = new c_body();

		$this->expect( '{' );
		while( !$s->ended() && $s->peek()->type != '}' ) {
			$part = $this->body_part();
			$body->add( $part );
		}
		$this->expect( '}' );
		return $body;
	}

	private static $constructs = array(
		'if', 'for', 'while', 'switch', 'return'
	);

	// <body-part>: (comment | <obj-def> | <construct>
	// 	| (<expr> ";") | <body> )...
	private function body_part()
	{
$this->trace( "body_part" );
		$s = $this->s;
		$t = $s->peek();

		// comment?
		if( $t->type == 'comment' ) {
			$s->get();
			return new c_comment( $t->content );
		}

		// <construct>?
		if( in_array( $t->type, self::$constructs ) ) {
			$func = 'read_' . $t->type;
			return $this->$func();
		}

		// <body>?
		if( $t->type == '{' ) {
			return $this->body();
		}

		// <varlist>?
		if($this->type_follows()) {
			$list = $this->read_varlist();
			$this->expect(';');
			return $list;
		}

		// <expr>
		$expr = $this->expr();
		$this->expect( ';' );
		return $expr;
	}

	private function read_varlist()
	{
$this->trace( "read_varlist" );
		$s = $this->s;

		$type = $this->type();
		$list = new c_varlist($type);

		$f = $this->form();
		$e = null;
		if($s->peek()->type == '=') {
			$s->get();
			$e = $this->expr();
		}
		$list->add($f, $e);

		while($s->peek()->type == ',') {
			$comma = $s->get();

			if(!$this->form_follows()) {
				$s->unget($comma);
				break;
			}
			$f = $this->form();
			$e = null;
			if($s->peek()->type == '=') {
				$s->get();
				$e = $this->expr();
			}
			$list->add($f, $e);
		}
		return $list;
	}

	private function form_follows()
	{
		if($this->type_follows()) return false;
		$t = $this->s->peek()->type;
		return $t == '*' || $t == 'word';
	}

	private function is_typename( $word )
	{
		return in_array( $word, $this->type_names );
	}

	private function type_follows()
	{
		$b = $this->_type_follows();
		return $b;

		$r = $b ? '+' : '';
		$c = $this->context();
		put("$r	$c");

		return $b;
	}

	private function _type_follows()
	{
$this->trace( "type_follows" );
		$t = $this->s->peek();

		$type_keywords = array( 'struct', 'const' );
		if( in_array( $t->type, $type_keywords ) ) {
			return true;
		}

		if( $t->type != 'word' ) {
			return false;
		}
		return $this->is_typename( $t->content );
	}

	function context( $n = 4 )
	{
		$s = $this->s;

		$buf = array();
		while( $n-- > 0 && !$s->ended() ) {
			$t = $s->get();
			$buf[] = $t;
		}

		$sbuf = array();
		foreach( $buf as $t ) {
			$c = $t->content;
			if( $c === null ) $c = $t->type;
			$sbuf[] = $c;
		}
		$str = implode( ' ', $sbuf );

		while( !empty( $buf ) ) {
			$s->unget( array_pop( $buf ) );
		}
		return $str;
	}

	private function read_if()
	{
$this->trace( "read_if" );
		$this->expect( 'if' );
		$this->expect( '(' );
		$expr = $this->expr();
		$this->expect( ')' );

		$body = $this->read_body_or_part();

		$t = $this->s->peek();
		if( $t->type == 'else' ) {
			$this->s->get();
			$else = $this->read_body_or_part();
		}
		else {
			$else = null;
		}

		return new c_if( $expr, $body, $else );
	}

	private function read_body_or_part()
	{
		if( $this->s->peek()->type == '{' ) {
			return $this->read_body();
		}

		$b = new c_body();
		$b->add( $this->body_part() );
		return $b;
	}

	private function read_for()
	{
$this->trace( "read_for" );
		$this->expect( 'for' );
		$this->expect( '(' );

		if($this->type_follows()) {
			$init = $this->read_varlist();
		}
		else {
			$init = $this->expr();
		}
		$this->expect( ';' );
		$cond = $this->expr();
		$this->expect( ';' );
		$act = $this->expr();
		$this->expect( ')' );

		$body = $this->read_body();

		return new c_for( $init, $cond, $act, $body );
	}

	private function read_while()
	{
$this->trace( "read_while" );
		$this->expect( 'while' );
		$this->expect( '(' );
		$cond = $this->expr();
		$this->expect( ')' );

		$body = $this->read_body();

		return new c_while( $cond, $body );
	}

	// <return>: "return" [<expr>] ";"
	private function read_return()
	{
$this->trace( "read_return" );
		$this->expect( 'return' );
		if( $this->s->peek()->type != ';' ) {
			$expr = $this->expr();
		} else {
			$expr = new c_expr();
		}
		$this->expect( ';' );
		return new c_return( $expr->parts );
	}

	// <switch>: "switch" "(" <expr> ")" "{" <switch-case>... "}"
	private function read_switch()
	{
$this->trace( "read_switch" );
		$this->expect( 'switch' );
		$this->expect( '(' );
		$cond = $this->expr();
		$this->expect( ')' );

		$s = $this->s;
		$cases = [];

		$this->expect( '{' );
		while( !$s->ended() && $s->peek()->type != '}' ) {
			$cases[] = $this->read_case();
		}
		$this->expect( '}' );

		return new c_switch( $cond, $cases );
	}

	// <switch-case>: "case" <literal>|<id> ":" <body-part>...
	private function read_case()
	{
$this->trace( "read_case" );
		$s = $this->s;

		$this->expect( 'case' );

		// <id> | <literal>
		$p = $s->peek();
		if( $p->type == 'word' ) {
			$val = new c_literal( $s->get()->content );
		}
		else {
			$val = $this->literal();
		}

		// :
		$this->expect( ':' );

		// <body-part>...
		$body = new c_body();
		while( !$s->ended() )
		{
			$t = $s->peek();
			if( $t->type == 'case' ) {
				break;
			}
			if( $t->type == '}' ) {
				break;
			}
			$body->add( $this->body_part() );
		}
		return array( $val, $body );
	}

	// <expr>: <atom> [<op> <atom>]...
	private function expr()
	{
$this->trace( "expr" );
		$s = $this->s;

		$e = new c_expr();
		$e->add( $this->atom() );

		while( !$s->ended() && $this->is_op( $s->peek() ) ) {
			$e->add( $s->get()->type );
			$e->add( $this->atom() );
		}

		return $e;
	}

	private function is_op( $t )
	{
		$ops = ['+', '-', '*', '/', '=',
			'|', '&', '~', '^',
			'<', '>', '?', ':', '%',
			'+=', '-=', '*=', '/=', '%=', '&=', '^=', '|=',
			'++', '--',
			'->', '.', '>>', '<<',
			'<=', '>=', '&&', '||', '==', '!=',
			'<<=', '>>=' ];
		return in_array( $t->type, $ops );
	}

	// <expr-atom>: <cast>? (
	// 		<literal> / <sizeof> /
	// 		<left-op>... ("(" <expr> ")" / <name>) <right-op>...
	// )
	private function atom()
	{
$this->trace( "atom" );
		$s = $this->s;


		$ops = array();

		// <cast>?
		if( $this->cast_follows() )
		{
			$this->expect( '(' );
			$tf = $this->read_typeform();
			$this->expect( ')' );
			$ops[] = array( 'cast', $tf );
		}

		// <literal>?
		if( $this->literal_follows() ) {
			$ops[] = array( 'literal', $this->literal() );
			return $ops;
		}

		// <sizeof>?
		if( $s->peek()->type == 'sizeof' ) {
			$ops[] = array( 'sizeof', $this->read_sizeof() );
			return $ops;
		}

		// <left-op>...
		$L = array( '&', '*', '++', '--', '!', '~' );
		while( !$s->ended() && in_array( $s->peek()->type, $L ) ) {
			$ops[] = array( 'op', $s->get()->type );
		}

		// "(" <expr> ")" / <name>
		if($s->peek()->type == '(') {
			$this->expect('(');
			$ops[] = array('expr', $this->expr());
			$this->expect(')');
		}
		else {
			$t = $this->expect('word');
			$ops[] = array('id', $t->content);
		}

		while( !$s->ended() )
		{
			$p = $s->peek();
			if( $p->type == '--' || $p->type == '++' ) {
				$ops[] = array( 'op', $s->get()->type );
				continue;
			}

			if( $p->type == '[' ) {
				$s->get();
				$ops[] = array( 'index', $this->expr() );
				$this->expect( ']' );
				continue;
			}

			if( $p->type == '.' || $p->type == '->' ) {
				$s->get();
				$ops[] = array( 'op', $p->type );
				$t = $s->get();
				if( $t->type != 'word' ) {
					return $this->error( "Id expected, got $t" );
				}
				$ops[] = array( 'id', $t->content );
				continue;
			}

			if( $p->type == '(' )
			{
				$s->get();
				$args = [];
				while( !$s->ended() && $s->peek()->type != ')' ) {
					$args[] = $this->expr();
					if( $s->peek()->type == ',' ) {
						$s->get();
					} else {
						break;
					}
				}
				$this->expect( ')' );
				$ops[] = array( 'call', $args );
			}

			break;
		}

		return $ops;
	}

	private function read_typeform()
	{
		$t = $this->type();
		$f = $this->form();
		return new c_typeform($t, $f);
	}

	private function cast_follows()
	{
		$s = $this->s;
		$buf = array();

		$t = $s->get();
		if( $t->type != '(' ) {
			$s->unget( $t );
			return false;
		}

		if( !$this->type_follows() ) {
			$s->unget( $t );
			return false;
		}

		$s->unget($t);
		return true;
	}

	// <sizeof>: "sizeof" <sizeof-arg>
	// <sizeof-arg>: ("(" (<expr> | <type>) ")") | <expr> | <type>
	private function read_sizeof()
	{
$this->trace( "read_sizeof" );
		$s = $this->s;
		$parts = [];

		$this->expect( 'sizeof' );

		$brace = false;
		if( $s->peek()->type == '(' ) {
			$brace = true;
			$s->get();
		}

		if( $this->type_follows() ) {
			$arg = $this->read_typeform();
		}
		else {
			$arg = $this->expr();
		}

		if( $brace ) {
			$this->expect( ')' );
		}

		return new c_sizeof($arg);
	}

	private function literal_follows()
	{
		$s = $this->s;
		$t = $s->peek()->type;
		return (
			$t == 'num' ||
			$t == 'string' ||
			$t == 'char' ||
			($t == '-' && $s->peek(1)->type == 'num') ||
			$t == '{'
		);
	}

	private function literal()
	{
$this->trace( "literal" );
		$t = $this->s->get();

		if( $t->type == 'num' ) {
			return new c_number( $t->content );
		}

		if( $t->type == 'string' ) {
			return new c_string( $t->content );
		}

		if( $t->type == 'char' ) {
			return new c_char( $t->content );
		}

		if( $t->type == '-' ) {
			$n = $this->expect( 'num' );
			return new c_number( '-'.$n->content );
		}

		if( $t->type == '{' )
		{
			$p = $this->s->peek();
			$this->s->unget( $t );

			if( $p->type == '.' ) {
				return $this->struct_literal();
			}
			else {
				return $this->array_literal();
			}
		}

		if( $t->type == 'word' ) {
			return new c_literal( $t->content );
		}

		if($t->type == '&') {
			$this->s->unget($t);
			return $this->addr_literal();
		}

		return $this->error( "Unexpected $t" );
	}

	// <struct-literal>: "{" "." <id> "=" <literal> [, ...] "}"
	private function struct_literal()
	{
$this->trace( "struct_literal" );
		$s = $this->s;

		$struct = new c_struct_literal();

		$this->expect( '{' );
		while( !$s->ended() )
		{
			$this->expect( '.' );
			$id = $this->expect( 'word' )->content;
			$this->expect( '=' );
			$val = $this->literal();

			$struct->add( $id, $val );

			if( $s->peek()->type == ',' ) {
				$s->get();
			} else {
				break;
			}
		}
		$this->expect( '}' );
		return $struct;
	}

	private function array_literal()
	{
$this->trace( "array_literal" );
		$s = $this->s;

		$elements = array();

		$this->expect( '{' );
		while( !$s->ended() && $s->peek()->type != '}' ) {
			$elements[] = $this->literal();
			if( $s->peek()->type == ',' ) {
				$s->get();
			}
		}
		$this->expect( '}' );

		return new c_array( $elements );
	}

	private function addr_literal()
	{
$this->trace("addr_literal");
		$s = $this->s;
		$str = '&';
		$this->expect('&');
		$str .= $this->expect('word')->content;
		return new c_literal($str);
	}

	private function expect( $ch, $content = null ) {
		$t = $this->s->get();
		if( $t->type != $ch ) {
			$this->error( "'$ch' expected, got $t" );
		}
		if( $content !== null && $t->content !== $content ) {
			$this->error( "[$ch, $content] expected, got $t" );
		}
		return $t;
	}

	//function error( $e ) { trigger_error( $e ); }
}

?>
