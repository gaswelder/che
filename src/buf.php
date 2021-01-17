<?php

require_once __DIR__ . "/rust-proxy.php";

class buf
{
	function __construct($s)
	{
		$this->proxy = make_rust_object('buf', $s);
	}

	function __call($f, $args)
	{
		return call_user_func_array([$this->proxy, $f], $args);
	}
}
