<?php

class c_import
{
    public $path;

    function format()
    {
        return "import $this->path\n";
    }

    function name()
    {
        return $this->path;
    }
}
