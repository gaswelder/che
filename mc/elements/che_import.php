<?php

class che_import
{
	private $path;
	private $dir;

	public function __construct($path, $dir)
	{
		$this->path = $path;
		$this->dir = $dir;
	}

	/**
	 * Returns the package corresponding to this import.
	 */
	function get_module() : package
	{
		return package::read($this->path, $this->dir);
	}

	function path()
	{
		return $this->path;
	}

	static function parse(parser $parser)
	{
		list($path) = $parser->seq('import', '$literal-string');
		$dir = dirname(realpath($parser->path));

		// Add types exported from the referenced module.
		$package = package::read($path->content, $dir);
		foreach ($package->types() as $types) {
			foreach ($types as $type) {
				$parser->add_type($type);
			}
		}

		return new self($path->content, $dir);
	}
}