<?php

abstract class che_element extends element
{
	/**
	 * Returns a list of elements equivalent to this element in C.
	 *
	 * @return array
	 */
	function translate()
	{
		return [$this];
	}
}
