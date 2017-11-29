<?php

/*
 * Returns path to the specified module.
 */
function find_import($name, $refdir)
{
	if ($name[0] == '.') {
		$name = substr($name, 1);
		$p = array($refdir.$name);
	}
	else {
		$p = array(
			MCDIR."/lib/$name",
			"$refdir/$name",
			$name
		);
	}
	foreach ($p as $path) {
		if (file_exists($path)) {
			return $path;
		}
		$path .= ".c";
		if (file_exists($path)) {
			return $path;
		}
	}
	return null;
}

?>
