<?php

/*
 * Form, an object description like *foo[42] or myfunc(int, int).
 */
class che_form
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
				$s = $mod . $s;
				continue;
			}

			if (is_string($mod) && strlen($mod) > 1 && $mod[0] == '[') {
				$s = $s . $mod;
				continue;
			}

			if ($mod instanceof che_formal_args) {
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
		return new che_form($f['name'], $f['ops']);
	}
}
