<?php
/*
 * Sets the debugging error handler.
 */
function stop_on_error()
{
	set_error_handler(function ($level, $msg, $file, $line) {
		if (!error_reporting()) return;
		fwrite(STDERR, "\nError: $msg at $file:$line\n\n");
		__debug::print_source($file, $line);
		__debug::print_stack();
		exit(1);
	});
}

function debmsg($m)
{
	__debug::message($m);
}

class __debug
{
	static $messages = false;

	static function message($m)
	{
		if (!self::$messages) return;
		echo "$m\n";
	}

	static function print_stack()
	{
		fwrite(STDERR, "Stack:\n");
		$stack = array_reverse(debug_backtrace());
		foreach ($stack as $i => $info) {
			$call = self::format_call($info);
			fwrite(STDERR, "$i\t$call\n");
		}
		fwrite(STDERR, "\n");
	}

	private static function format_call($r)
	{
		$f = '';
		if (isset($r['class'])) {
			$f .= "$r[class]$r[type]";
		}
		$f .= $r['function'];
		$f .= '(' . self::format_args($r['args']) . ')';
		if (isset($r['line'])) {
			$f .= " / " . basename($r['file']) . ':' . $r['line'];
		}
		return $f;
	}

	private static function format_args($args)
	{
		$parts = array();
		foreach ($args as $arg) {
			if (!is_scalar($arg)) {
				$parts[] = gettype($arg);
				continue;
			}

			if (is_string($arg)) {
				if (mb_strlen($arg) > 20) {
					$arg = mb_substr($arg, 0, 17) . '...';
				}
				$arg = "'" . $arg . "'";
			}
			$parts[] = $arg;
		}
		return implode(', ', $parts);
	}

	static function print_source($file, $line)
	{
		/*
		 * Define the range of lines to print.
		 */
		$margin = 4;
		$line--;
		// make it count from zero.
		$l1 = $line - $margin;
		if ($l1 < 0) $l1 = 0;
		$l2 = $line + $margin;

		$lines = file($file);
		$n = count($lines);
		if ($l2 >= $n) {
			$l2 = $n - 1;
		}
		for ($i = $l1; $i <= $l2; $i++) {
			/*
			 * If this is the error line, mark it with an arrow.
			 */
			if ($i == $line) {
				fwrite(STDERR, "->");
			}
			/*
			 * Print the line number and the line itself.
			 */
			fprintf(STDERR, "\t%d\t%s", $i + 1, $lines[$i]);
		}
		fwrite(STDERR, "\n");
	}
}

?>
