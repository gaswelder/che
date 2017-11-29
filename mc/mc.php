<?php
$dir = dirname(__FILE__);
require $dir.'/debug.php';
stop_on_error();
error_reporting(-1);
require $dir.'/buf.php';
require $dir.'/parser/parser.php';
require $dir.'/objects.php';
require $dir.'/translator.php';
require $dir.'/tr_headers.php';
require $dir.'/module.php';
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
		$mod = module::parse($this->main, []);

		$sources = [];
		$link = [];

		$all = $this->modules_list($mod);
		foreach ($all as $mod) {
			mc_trans::translate($mod);
			$tmppath = tmppath($mod->path);
			file_put_contents($tmppath, $mod->format_as_c());
			$sources[] = $tmppath;
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
			$list = array_merge($list, $this->modules_list($dep));
		}
		return $list;
	}
}


function tmppath($path)
{
	$dir = sys_get_temp_dir().'/chetmp';
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
