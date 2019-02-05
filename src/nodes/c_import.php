<?php

class c_import
{
    private $path;

    static function parse($lexer)
    {
        expect($lexer, 'import');
        $tok = expect($lexer, 'string');
        // expect($lexer, ';');
        $self = new self;
        $self->path = $tok->content;
        return $self;
    }

    function format()
    {
        return "import $this->path\n";
    }

    function name()
    {
        return $this->path;
    }
}
