<?php
class mc_trans
{
	/*
	 * Translates Che code into C code
	 */
	static function translate(module $mod)
	{
		$code = $mod->code;
		
		$headers = array();
		$prototypes = array();
		$types = array();
		$body = array();

		$n = count($code);
		for ($i = 0; $i < $n; $i++) {
			$element = $code[$i];

			if ($element instanceof c_include || $element instanceof c_define) {
				$headers[] = $element;
				continue;
			}

			if ($element instanceof c_structdef || $element instanceof c_enum
				|| $element instanceof c_typedef) {
				$types[] = $element;
				continue;
			}

			if ($element instanceof c_import) {
				$imp = get_import($element->path, $element->dir);
				array_splice($code, $i, 1, $imp->code);
				$i--;
				$n = count($code);
				continue;
			}

			if ($element instanceof c_func) {
				$element = self::rewrite_func($element);
				$prototypes[] = $element->proto;
			}

			/*
			 * Mark module-global variables as static.
			 */
			if ($element instanceof c_varlist) {
				$element->stat = true;
			}

			$body[] = $element;
		}

		$out = array_merge($headers, $types, $prototypes, $body);
		$mod->code = $out;
		tr_headers::add_headers($mod);
	}

	private static function rewrite_func(c_func $f)
	{
		$n = count($f->body->parts);
		if (!$n) return $f;

		/*
		 * Make sure that every function's body ends with a return
		 * statement so that the deferred statements will be written
		 * there.
		 */
		if ($f->proto->form->name != 'main' && !($f->body->parts[$n-1] instanceof c_return)) {
			$f->body->parts[] = new c_return();
		}

		$f->body = self::rewrite_body($f->body);
		return $f;
	}

	private static function rewrite_body(c_body $b, $defer = array())
	{
		$c = new c_body();

		foreach ($b->parts as $part) {
			if ($part instanceof c_defer) {
				$defer[] = $part->expr;
				continue;
			}

			if ($part instanceof c_if || $part instanceof c_for || $part instanceof c_while) {
				$part = clone $part;
				$part->body = self::rewrite_body($part->body, $defer);
			}
			else if ($part instanceof c_switch) {
				$part = clone $part;
				foreach ($part->cases as $i => $case) {
					$part->cases[$i][1] = self::rewrite_body($part->cases[$i][1],
						$defer);
				}
			}
			else if ($part instanceof c_return) {
				foreach ($defer as $e) {
					$c->parts[] = $e;
				}
			}
			$c->parts[] = $part;
		}

		return $c;
	}
}

?>
