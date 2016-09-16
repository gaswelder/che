<?php

// Second stage parser that returns CPOM objects

function trace( $m, $s = null ) {
	fwrite( STDERR, "--- $m		$s\n" );
}

function put( $s ) {
	fwrite( STDERR, "$s\n" );
}

class ctok extends toksbasech
{
	private $s;
	private $path;

	function __construct( ctok1 $s, $path ) {
		parent::__construct($s);
		$this->s = $s;
		$this->path = $path;
	}

	private function trace( $m ) {
		return;
		$s = $this->context();
		fwrite( STDERR, "--- $m\t$s\n" );
	}

	protected function read()
	{
		if( $this->s->ended() ) {
			return null;
		}

		$s = $this->s;
		$t = $this->s->peek();

		// <macro>?
		if( $t->type == 'macro' ) {
			return $this->read_macro();
		}

		// <typedef>?
		if( $t->type == 'typedef' ) {
			$def = $this->read_typedef();
			return $def;
		}

		// <struct-def>?
		if( $t->type == 'struct' ) {
			if( $s->peek(1)->type == 'word' && $s->peek(2)->type == '{' ) {
				$def = $this->struct_def();
				return $def;
			}
		}

		// <enum-def>?
		if( $t->type == 'enum' ) {
			return $this->read_enumdef();
		}

		// comment?
		if( $t->type == 'comment' ) {
			$this->s->get();
			return new c_comment( $t->content );
		}

		if( $t->type == 'import' ) {
			$this->s->get();
			$path = $this->s->get();
			return new c_import( $path->content );
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

	// <typedef>: "typedef" <nameform> ";"
	private function read_typedef()
	{
$this->trace( "read_typedef" );
		$this->expect( 'typedef' );
		$form = $this->read_nameform();
		$this->expect( ';' );
		return new c_typedef( $form->name, $form->type );
	}

	// <struct-def>: "struct" <name> [<struct-fields>] ";"
	private function struct_def()
	{
$this->trace( "struct_def" );
		$this->expect( 'struct' );
		$name = $this->expect( 'word' )->content;
		$def = new c_structdef( $name );

		if( $this->s->peek()->type == ';' ) {
			$this->s->get();
			return $def;
		}

		$fields = $this->struct_fields();
		foreach( $fields as $f ) {
			$def->add( $f );
		}

		$this->expect( ';' );
		return $def;
	}

	// <enum-def>: "enum" "{" <id> ["=" <literal>] [,]... "}" ";"
	private function read_enumdef()
	{
		$s = $this->s;
		$this->expect( 'enum' );
		$this->expect( '{' );

		$enum = new c_enum();
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
	// <stor> <nameform> [= <expr>] ";"
	// or <stor> <nameform/func> ";"
	// or <stor> <nameform/func> <body>
	private function read_object()
	{
$this->trace( "read_object" );
		$s = $this->s;

		// <stor>
		$stor = [];
		$t = $s->peek();
		if( $t->type == 'static' ) {
			$stor[] = 'static';
			$s->get();
		}

		// <nameform>
		$form = $this->read_nameform();

		if( !$this->is_function( $form ) )
		{
			$def = new c_vardef( $form->name, $form->type );

			// [= <expr>] ";"
			if( $s->peek()->type == '=' ) {
				$s->get();
				$expr = $this->expr();
				$def->init = $expr;
			}

			$this->expect( ';' );
			return $def;
		}

		$type = $form->type;
		$args = array_shift( $type );
		assert( $args[0] == 'call' );
		array_shift( $args );
		$proto = new c_prototype( $type, $form->name, $args );

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

	// <struct-fields>: "{" [<nameform> ";"]... "}"
	private function struct_fields()
	{
$this->trace( "struct fields" );
		$s = $this->s;
		$fields = array();
		$this->expect( '{' );
		while( !$s->ended() && $s->peek()->type != '}' )
		{
			$form = $this->read_nameform();
			if( !$form || !$form->name || !$form->type ) {
				return $this->error( "Invalid nameform" );
			}
			$this->expect( ';' );

			$fields[] = new c_nameform( $form->name, $form->type );
		}
		$this->expect( '}' );
		return $fields;
	}

	// <nameform>: "const"? <type> <obj-der>
	private function read_nameform()
	{
$this->trace( "read_nameform" );
		$s = $this->s;

		$cast = [];

		// "const"?
		$t = $s->peek();
		if( $t->type == 'const' ) {
			$cast[] = 'const';
			$s->get();
		}

		// <type>
		$cast = array_merge( $cast, $this->type() );

		// <obj-der>
		$der = $this->obj_der();

		$type = array_merge( $der['ops'], array_reverse( $cast ) );
		$name = $der['name'];

		return new c_nameform( $name, $type );
	}

	// <type>: "struct" <name>
	//	or "struct" <struct-fields>
	//	or <type-mod>...
	private function type()
	{
$this->trace( "type" );
		$s = $this->s;

		$t = $s->get();

		if( $t->type == 'struct' )
		{
			// "struct" <name>?
			if( $s->peek()->type == 'word' ) {
				$t = $s->get();
				return array( $t->content, 'struct' );
			}

			// "struct" <struct-fields>
			return array( $this->struct_fields(), 'struct' );
		}

		// <type-mod>...
		$type = array();
		$type[] = $t->content;

		$t = $s->peek();
		while( $t->type == 'word' && $this->is_typename( $t->content ) ) {
			$type[] = $t->content;
			$s->get();
			$t = $s->peek();
		}
		return $type;
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


		/*
		// <left-mod>...
		while( !$s->ended() && $s->peek()->type != 'word' )
		{
			$t = $this->s->get();

			if( $t->type == '(' ) {
				$mod *= 10;
				continue;
			}

			if( $t->type == '*' ) {
				$left[] = array( '*', 1 * $mod );
				continue;
			}

			return $this->error( "Unexpected $t" );
		}

		// <name>
		$name = $this->expect( 'word' )->content;
		*/

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
				$conv .= $this->expr()->format();
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

	// <call-signature>: "(" <nameform> [",", <nameform>]... ")"
	private function call_signature()
	{
$this->trace( "call_signature" );
		$s = $this->s;

		$this->expect( '(' );

		$args = ['call'];

		while( !$s->ended() && $s->peek()->type != ')' )
		{
			if( $s->peek()->type == '...' ) {
				$args[] = '...';
				$s->get();
				break;
			}

			$args[] = $this->read_nameform();

			if( $s->peek()->type == ',' ) {
				$this->s->get();
			}
		}
		$this->expect( ')' );
		return $args;
	}

	private function is_function( c_nameform $f )
	{
$this->trace( "is_function" );
		return !empty( $f->type )
			&& is_array( $f->type[0] ) && $f->type[0][0] == 'call';
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

		// <expr>?
		if( $this->expr_follows() ) {
			$expr = $this->expr();
			$this->expect( ';' );
			return $expr;
		}

		// <obj-def>
		return $this->read_object();
	}

	private function expr_follows()
	{
		$b = $this->_expr_follows();
		return $b;

		$r = $b ? '+' : 'nope';
		$c = $this->context();
		put("$r	$c");

		return $b;
	}

	/*
	 * If what follows until ';' has at least one operator or brace,
	 * and there is less than two "words" before it, it's an expression.
	 */
	private function _expr_follows()
	{
$this->trace( "expr_follows" );
		if( $this->type_follows() ) {
			return false;
		}

		$s = $this->s;

		$buf = array();
		$n = -1;
		while( !$s->ended() && $s->peek()->type != ';' ) {
			$buf[] = $s->get();
			$n++;
		}
		while( $n >= 0 ) {
			$s->unget( $buf[$n] );
			$n--;
		}

		$op = 0;
		$eq = 0;
		$words = 0;
		foreach( $buf as $t )
		{
			if( $t->type == '=' ) {
				$eq++;
			}
			else if( $t->type == '(' || $this->is_op( $t )
				|| $t->type == '[' ) {
				$op++;
			}
			else if( $t->type == 'word' ) {
				$words++;
			}

			if( $eq == 1 && $op > 0 ) {
				return false;
			}

			if( ($op + $eq) == 1 && $words < 2 ) {
				return true;
			}
		}

		return ($op + $eq > 0) && $words < 2;
	}

	private static $type_names = array(
		'struct', 'enum',
		'void', 'char', 'short', 'int', 'long', 'float', 'double',
		'unsigned',
		'bool', 'va_list', 'FILE',
		'ptrdiff_t', 'size_t', 'wchar_t',
		'int8_t', 'int16_t', 'int32_t', 'int64_t',
		'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
		'clock_t', 'time_t'
	);

	private function is_typename( $word )
	{
		return in_array( $word, self::$type_names );
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

		$type_keywords = array( 'struct', 'const', 'static' );
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
		$s = $this->upstream;

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

		$init = $this->expr();
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

	// <return>: "return" <expr> ";"
	private function read_return()
	{
$this->trace( "read_return" );
		$this->expect( 'return' );
		$expr = $this->expr();
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
			'->', '.',
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
		$p = $s->peek();

		$ops = array();

		// <cast>?
		if( $this->cast_follows() )
		{
			$this->expect( '(' );
			$t = $this->read_nameform();
			$this->expect( ')' );
			$ops[] = array( 'cast', $t->type );
		}

		// <literal>?
		if( $this->literal_follows() ) {
			$ops[] = array( 'literal', $this->literal() );
			return $ops;
		}

		// <sizeof>?
		if( $p->type == 'sizeof' ) {
			$ops[] = array( 'sizeof', $this->read_sizeof() );
			return $ops;
		}

		// <left-op>...
		$L = array( '&', '*', '++', '--', '!', '~' );
		while( !$s->ended() && in_array( $s->peek()->type, $L ) ) {
			$ops[] = array( 'op', $s->get()->type );
		}

		// "(" <expr> ")" / <name>
		$t = $s->get();
		if( $t->type == '(' ) {
			$ops[] = array( 'expr', $this->expr() );
			$this->expect( ')' );
		}
		else {
			if( $t->type != 'word' ) {
				return $this->error( "Identifier expected, got $t" );
			}
			$ops[] = array( 'id', $t->content );
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

/*
		$n = 0;
		while( !$s->ended() && $n < 3 ) {
			$n++;
			$buf[] = $s->get();
		}

		$ok = ( $n == 3 && $buf[0]->type == '(' && $buf[1]->type == 'word'
			&& $buf[2]->type == ')' );
		while( $n > 0 ) {
			$n--;
			$s->unget( array_pop( $buf ) );
		}
		return $ok; */
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
			$nf = $this->read_nameform();
			assert( $nf->name == '' );
			$arg = array( 'type', $nf->type );
		}
		else {
			$arg = array( 'expr', $this->expr() );
		}

		if( $brace ) {
			$this->expect( ')' );
		}

		return new c_sizeof( $arg );
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

		if( $t->type == '{' ) {
			$this->s->unget( $t );
			return $this->array_literal();
		}

		return $this->error( "Unexpected $t" );
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

	private function expect( $ch, $content = null ) {
		$t = $this->s->get();
		if( $t->type != $ch ) {
			$this->error( "$ch expected, got $t" );
		}
		if( $content !== null && $t->content !== $content ) {
			$this->error( "[$ch, $content] expected, got $t" );
		}
		return $t;
	}

	//function error( $e ) { trigger_error( $e ); }
}

?>
