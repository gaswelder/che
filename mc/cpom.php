<?php
// Program object model

class c_element {
	function __toString() {
		$t = get_class( $this );
		$s = json_encode( $this, JSON_PRETTY_PRINT );
		return "$t$s";
	}
}

class c_macro extends c_element {
	public $type;
	public $arg = null;
	function __construct( $type, $arg = null ) {
		$this->type = $type;
		$this->arg = $arg;
	}
	function format() {
		$s = $this->type;
		if( $this->arg !== null ) {
			$s .= " $this->arg";
		}
		return $s;
	}
}

class c_define extends c_element {
	public $name;
	public $value;
	function __construct( $name, $value ) {
		$this->name = $name;
		$this->value = $value;
	}

	function format() {
		return "#define $this->name $this->value\n";
	}
}

class c_include extends c_element {
	public $path;
	function __construct( $path ) {
		$this->path = $path;
	}

	function format() {
		return "#include $this->path\n";
	}
}

class c_nameform extends c_element {
	public $name;
	public $type;
	function __construct( $name, $type ) {
		$this->name = $name;
		$this->type = $type;
	}

	function format( $tab = 0 ) {
		$s = $this->name;

		$n = count( $this->type );
		$i = 0;
		while( $i < $n )
		{
			$mod = $this->type[$i++];
			if( $mod == '*' ) {
				$s = $mod . $s;
				continue;
			}

			if( is_string( $mod) && strlen( $mod ) > 1 && $mod[0] == '[' ) {
				$s = $s . $mod;
				continue;
			}

			if( is_array( $mod ) && $mod[0] == 'call' )
			{
				$args = array_slice( $mod, 1 );
				$str = '(';
				foreach( $args as $j => $arg ) {
					if( $j > 0 ) $str .= ', ';
					if( $arg == '...' ) {
						$str .= '...';
					}
					else {
						$str .= $arg->format();
					}
				}
				$str .= ')';
				$s = $s . $str;
				continue;
			}

			if( $mod == 'struct' ) {
				$mod = $this->type[$i++];
				if( is_string( $mod ) ) {
					$s = "struct $mod $s";
					continue;
				}
			}

			if( is_string( $mod ) ) {
				$s = "$mod $s";
				continue;
			}

			echo '-----------------', "\n";
			var_dump( $mod );
			echo '-----------------', "\n";
			var_dump( $this->type );
			echo '-----------------', "\n";
			exit;
		}

		$p = str_repeat( "\t", $tab );
		return $p . $s;
	}
}

class c_typedef extends c_nameform {
	function format( $tab = 0 ) {
		return 'typedef ' . parent::format() . ';';
	}
};

class c_prototype extends c_nameform
{
	public $type;
	public $name;
	public $args;

	function __construct( $rettype, $name, $args ) {
		$this->type = $rettype;
		$this->name = $name;
		$this->args = $args;
	}

	function format( $tab = 0 )
	{
		$pref = str_repeat( "\t", $tab );

		$s = $this->format_type() . ' ' . $this->name;
		$s .= $this->format_args();
		$s .= ';';
		return $s;
		//return parent::format( $tab ) . ';';
	}

	private function format_type()
	{
		$n = count( $this->type );
		$i = 0;
		$s = '';
		while( $i < $n )
		{
			$mod = $this->type[$i++];
			if( $mod == '*' ) {
				$s = $mod . $s;
				continue;
			}

			if( is_string( $mod ) && strlen( $mod ) > 1 && $mod[0] == '[' ) {
				$s = $s . $mod;
				continue;
			}

			if( $mod == 'struct' ) {
				$mod = $this->type[$i++];
				if( is_string( $mod ) ) {
					$s = "struct $mod $s";
					continue;
				}
			}

			if( is_string( $mod ) ) {
				$s = "$mod $s";
				continue;
			}

			echo '-----------------', "\n";
			var_dump( $mod );
			echo '-----------------', "\n";
			var_dump( $this->type );
			echo '-----------------', "\n";
			exit;
		}

		return $s;
	}

	private function format_args()
	{
		$s = '(';
		foreach( $this->args as $i => $arg )
		{
			if( $i > 0 ) $s .= ', ';
			if( $arg == '...' ) {
				$s .= '...';
			}
			else {
				$s .= $arg->format();
			}
		}
		$s .= ')';
		return $s;
	}
};

class c_structdef extends c_element {
	public $name;
	public $fields = array(); // array of nameforms

	function __construct( $name ) {
		$this->name = $name;
	}

	function add( c_nameform $form ) {
		$name = $form->name;
		$this->fields[] = $form;
	}

	function format() {
		$s = "struct $this->name";
		if( empty( $this->fields ) ) {
			$s .= ";";
			return $s;
		}

		$s .= " {\n";
		foreach( $this->fields as $form ) {
			$s .= "\t" . $form->format() . ";\n";
		}
		$s .= "}";
		return $s;
	}
}

class c_vardef extends c_nameform {
	public $init;
	function format( $tab = 0 ) {
		$s = parent::format( $tab );
		if( $this->init ) {
			$s .= ' = ' . $this->init->format();
		}
		return $s;
	}
}

class c_literal extends c_element {
	public $content;
	function __construct( $s ) {
		$this->content = $s;
	}
}

class c_string extends c_literal{
	function format() {
		return '"' . $this->content . '"';
	}
};

class c_char extends c_literal{
	function format() {
		return "'" . $this->content . "'";
	}
};

class c_number extends c_literal{
	function format() {
		return $this->content;
	}
};
class c_array extends c_literal{
	function format()
	{
		$i = 0;
		$s = '{';
		foreach( $this->content as $e )
		{
			if( $i > 0 ) {
				$s .= ', ';
			}
			$i++;

			$s .= $e->format();
		}
		$s .= '}';
		return $s;
	}
};
class c_sizeof extends c_literal{
	function format() {
		$s = 'sizeof (';
		if( $this->content[0] == 'type' ) {
			$s .= implode( ' ', $this->content[1] );
		}
		else {
			$s .= $this->content[1]->format();
		}
		$s .= ')';
		return $s;
	}
};


class c_expr extends c_element {
	public $parts;
	function __construct( $parts = array() ) {
		if( !is_array( $parts ) ) {
			var_dump( $parts );
			trigger_error( "Not an array" );
		}
		$this->parts = $parts;
	}
	function add( $p ) {
		$this->parts[] = $p;
	}

	function format( $tab = 0 ) {
		$p = str_repeat( "\t", $tab );
		$n = count( $this->parts );
		$i = 0;
		$s = '';
		while( $i < $n ) {
			$a = $this->parts[$i];
			$s .= $this->format_atom( $a );
			$i++;
			if( $i < $n ) {
				$s .= ' ' . $this->parts[$i] . ' ';
				$i++;
			}
		}
		return $p . $s;
	}

	function format_atom( $a )
	{
		$s = '';
		foreach( $a as $i )
		{
			switch( $i[0] ) {
				case 'id':
				case 'op':
					$s .= $i[1];
					break;
				case 'call':
					$s .= '(';
					$n = 0;
					foreach( $i[1] as $expr ) {
						if( $n > 0 ) $s .= ', ';
						$s .= $expr->format();
						$n++;
					}
					$s .= ')';
					break;
				case 'literal':
					$s .= $i[1]->format();
					break;
				case 'index':
					$s .= '[' . $i[1]->format() . ']';
					break;
				case 'sizeof':
					$s .= $i[1]->format();
					break;
				case 'cast':
					$s .= '(';
					$list = array_reverse( $i[1] );
					foreach( $list as $k => $cast ) {
						if( $k > 0 ) $s .= ' ';
						$s .= $cast;
					}
					$s .= ')';
					break;
				case 'expr':
					$s .= '(' . $i[1]->format() . ')';
					break;
				default:
					var_dump( $i );
					exit;
			}
		}
		return $s;
	}
}
class c_return extends c_expr {
	function format($tab = 0) {
		return str_repeat( "\t", $tab ) . 'return ' . parent::format();
	}
};

class c_body {
	public $parts; // whatever
	function add( $p ) {
		$this->parts[] = $p;
	}

	function format( $tab = 0 ) {
		$pref = str_repeat( "\t", $tab );
		$s = "{\n";
		foreach( $this->parts as $part ) {
			$s .= $part->format( $tab + 1 );
			if( !self::is_construct( $part ) ) {
				$s .= ';';
			}
			$s .= "\n";
		}
		$s .= "$pref}\n";
		return $s;
	}

	static function is_construct( $part ) {
		$a = ['c_if', 'c_for', 'c_while', 'c_switch'];
		return in_array( get_class( $part ), $a );
	}
}

class c_comment extends c_literal {
	function format() {
		return "/* $this->content */";
	}
};

class c_if {
	public $cond; // c_expr
	public $body; // c_body
	public $else;
	function __construct( $cond, $body, $else ) {
		$this->cond = $cond;
		$this->body = $body;
		$this->else = $else;
	}

	function format( $tab = 0 )
	{
		$pref = str_repeat( "\t", $tab );
		$s = $pref . sprintf( "if (%s) ", $this->cond->format() );
		$s .= $this->body->format( $tab );
		if( $this->else ) {
			$s .= $pref . "else " . $this->else->format( $tab );
		}
		return $s;
	}
}

class c_switch {
	private $cond;
	private $cases;

	function __construct( $cond, $cases ) {
		$this->cond = $cond;
		$this->cases = $cases;
	}

	function format( $tab = 0 ) {
		$pref = str_repeat( "\t", $tab );

		$s = $pref . sprintf( "switch (%s) {\n", $this->cond->format() );

		foreach( $this->cases as $case )
		{
			$s .= $pref . "case $case[0]:\n";
			foreach( $case[1] as $part ) {
				$s .= $part->format( $tab );
			}
		}

		$s .= "}\n";
	}
}

class c_for {
	public $init; // c_expr
	public $cond; // c_expr;
	public $act;
	public $body;
	function __construct( $i, $c, $a, $b ) {
		$this->init = $i;
		$this->cond = $c;
		$this->act = $a;
		$this->body = $b;
	}

	function format( $tab = 0 ) {
		$p = str_repeat( "\t", $tab );
		$s = $p . sprintf( "for (%s; %s; %s) ",
			$this->init->format(),
			$this->cond->format(),
			$this->act->format()
		);
		$s .= $this->body->format( $tab );
		return $s;
	}
}

class c_while {
	public $cond;
	public $body;
	function __construct( $cond, $body ) {
		$this->cond = $cond;
		$this->body = $body;
	}
	function format( $tab = 0 ) {
		$p = str_repeat( "\t", $tab );
		$s = $p . sprintf( "while (%s) ", $this->cond->format() );
		$s .= $this->body->format( $tab );
		return $s;
	}
}

class c_func extends c_element
{
	public $proto;
	public $body;

	function __construct( $proto, $body )
	{
		$this->proto = $proto;
		$this->body = $body;
	}

	function format( $tab = 0 )
	{
		$s = $this->proto->format();
		$s = substr( $s, 0, -1 ); // remove ';'
		$s .= ' ';
		$s .= $this->body->format();
		return $s;
	}
}

?>
