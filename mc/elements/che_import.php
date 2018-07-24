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
	 * Returns the module or the package corresponding to this import.
	 */
	function get_module()
	{
		return module::import($this->path, $this->dir);
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
		$imp = module::import($path->content, $dir);
		foreach ($imp->synopsis() as $decl) {
			if ($decl instanceof c_typedef) {
				$parser->add_type($decl->form->name);
			}
		}
		return new self($path->content, $dir);
	}
}