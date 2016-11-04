<?php
$dir = dirname(__FILE__);
require $dir.'/debug.php';
stop_on_error();
error_reporting(-1);
require $dir.'/buf.php';
require $dir.'/tokens.php';
require $dir.'/parser.php';
require $dir.'/objects.php';
require $dir.'/translator.php';
require $dir.'/tr_headers.php';
require $dir.'/modules.php';
require $dir.'/packages.php';

function trace($m, $s = null)
{
	fwrite(STDERR, "--- $m		$s\n");
}

function put($s)
{
	fwrite(STDERR, "$s\n");
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
	if (count($args) != 1) {
		fprintf(STDERR, "Usage: che <main file>\n");
		return 1;
	}

	$path = array_shift($args);
	compile($path);
}

function compile($main)
{
	/*
	 * The modules list starts with the main source file.
	 * Parse the modules, adding newly discovered
	 * dependencies to the end of the list.
	 */
	$link = array();
	$list = array($main);
	$n = count($list);
	for ($i = 0; $i < $n; $i++) {
		$mod = parse_module($list[$i]);
		foreach ($mod->deps as $path) {
			$list[] = $path;
			$n++;
		}
		$link = array_merge($link, $mod->link);
	}

	/*
	 * After all the dependencies have been parsed, the list may contain
	 * duplicate names due to common dependencies. The duplicates have
	 * to be removed leaving ones closer to the end of the list, so that
	 * [main, a, b, a, c] would become [main, b, a, c].
	 */
	$rev = array_reverse($list);
	$list = array();
	foreach ($rev as $name) {
		if (!in_array($name, $list)) {
			$list[] = $name;
		}
	}
	$list = array_reverse($list);

	/*
	 * Translate the modules to C
	 */
	$sources = array();
	foreach ($list as $path) {
		$mod = parse_module($path);
		$code = mc_trans::translate($mod->code);
		$tmppath = tmppath($path);
		file_put_contents($tmppath, format_che($code));
		$sources[] = $tmppath;
	}

	/*
	 * Derive executable name from the main module
	 */
	$outname = basename($list[0]);
	if ($p = strrpos($outname, '.')) {
		$outname = substr($outname, 0, $p);
	}

	/*
	 * Run the compiler
	 */
	exit(c99($sources, $outname, $link));
}

function tmppath($path)
{
	$dir = 'mcbuild';
	if (!file_exists($dir) && !mkdir($dir)) {
		exit(1);
	}
	return $dir.'/'.basename($path).'-'.md5($path).'.c';
}

function c99($sources, $name, $link)
{
	$cmd = 'c99 -Wall -Wextra -Werror -pedantic -pedantic-errors';
	$cmd .= ' -fmax-errors=3';
	$cmd .= ' -g '.implode(' ', $sources);
	$cmd .= ' -o '.$name;
	foreach ($link as $name) {
		$cmd .= ' -l '.$name;
	}
	exec($cmd, $output, $ret);
	return $ret;
}

/*
 * Formats given Che code as text.
 */
function format_che($code)
{
	$term = array(
		'c_typedef',
		'c_structdef',
		'c_varlist'
	);
	$out = '';
	foreach ($code as $element) {
		$out .= $element->format();
		$cn = get_class($element);
		if (in_array($cn, $term)) {
			$out .= ';';
		}
		$out .= "\n";
	}
	return $out;
}

?>
