<?php
require __DIR__ . '/debug.php';
stop_on_error();
error_reporting(-1);

spl_autoload_register(function ($classname) {
	$dir = __DIR__;
	$paths = [
		"$dir/elements/$classname.php",
		"$dir/module/$classname.php",
		"$dir/parser/$classname.php"
	];
	foreach ($paths as $path) {
		if (file_exists($path)) {
			require $path;
			return;
		}
	}
});

require __DIR__ . '/parser/subparsers.php';
require __DIR__ . '/parser/literals.php';
require __DIR__ . '/parser/expressions.php';
require __DIR__ . '/parser/signatures.php';
require __DIR__ . '/parser/control.php';
require __DIR__ . '/objects.php';

class trace
{
	static $enabled = false;

	static function line($s)
	{
		if (!self::$enabled) {
			return;
		}
		fwrite(STDERR, "$s\n");
	}
}

function mc_main($args)
{
	$home = getenv('CHE_HOME');
	if ($home === false) {
		$home = '.';
	}
	define('MCDIR', $home);
	define('CHE_OS', 'unix');

	array_shift($args);
	if (!empty($args) && $args[0] == '-t') {
		trace::$enabled = true;
		array_shift($args);
	}

	if (count($args) != 1) {
		fprintf(STDERR, "Usage: che <main file>\n");
		return 1;
	}

	$path = array_shift($args);
	$p = new program($path);
	$p->compile();
}

class program
{
	private $main;

	function __construct($main)
	{
		$this->main = $main;
	}

	function compile()
	{
		$package = package::read($this->main);
		$main = $package->merge();

		$sources = [];
		$link = [];

		$all = $this->modules_list($main);
		foreach ($all as $mod) {
			$mod = $mod->translate();
			$sources[] = $mod->write();
			$link = array_merge($link, $mod->link);
		}

		/*
		 * Derive executable name from the main module
		 */
		$outname = basename($this->main);
		if ($p = strrpos($outname, '.')) {
			$outname = substr($outname, 0, $p);
		}

		/*
		 * Run the compiler
		 */
		exit(c99($sources, $outname, $link));
	}

	private function modules_list(module $m)
	{
		$list = [$m];
		foreach ($m->deps as $dep) {
			$list = array_merge($list, $this->modules_list($dep->merge()));
		}
		// Omit duplicate modules, leaving the ones closer to the end.
		return array_reverse(array_unique(array_reverse($list), SORT_REGULAR));
	}
}

function c99($sources, $name, $link)
{
	$cmd = 'c99 -Wall -Wextra -Werror -pedantic -pedantic-errors';
	$cmd .= ' -fmax-errors=3';
	$cmd .= ' -g ' . implode(' ', $sources);
	$cmd .= ' -o ' . $name;
	foreach ($link as $name) {
		$cmd .= ' -l ' . $name;
	}
	exec($cmd, $output, $ret);
	return $ret;
}
