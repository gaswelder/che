<?php

class che_switch
{
	public $cond;
	public $cases;

	function format($tab = 0)
	{
		$pref = str_repeat("\t", $tab);

		$s = $pref . sprintf("switch (%s) {\n", $this->cond->format());

		foreach ($this->cases as $case) {
			$s .= $case->format();
		}

		$s .= "}\n";
		return $s;
	}

	static function parse(parser $parser)
	{
		$sw = new che_switch();
		list($cond) = $parser->seq('switch', '(', '$expr', ')', '{');

		$sw->cond = $cond;

		while (!$parser->s->ended() && $parser->s->peek()->type != '}') {
			$sw->cases[] = $parser->read('switch-case');
		}
		$parser->expect('}');
		return $sw;
	}
}

class che_switch_case extends che_element
{
	public $value;
	public $body;

	function format()
	{
		return sprintf("case %s:\n%s\nbreak;", $this->value->format(), $this->body->format());
	}

	static function parse(parser $parser)
	{
		$parser->expect('case');
		
		// <id> | <literal>
		$val = $parser->any(['identifier', 'literal']);
		$parser->expect(':');
	
		// <body-part>...
		$body = new che_body();
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
