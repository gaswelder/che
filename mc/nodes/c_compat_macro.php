<?php

class c_compat_macro
{
    private $content;

    static function parse($lexer)
    {
        $self = new self;
        $self->content = expect($lexer, 'macro')->content;
        return $self;
    }

    function format()
    {
        return $this->content . "\n";
    }

    private function kv()
    {
        $pos = strpos($this->content, ' ');
        if ($pos === false) {
            throw new Exception("can't get macro name from '$this->content'");
        }
        return [
            substr($this->content, 1, $pos - 1),
            substr($this->content, $pos + 1)
        ];
    }

    function name()
    {
        return $this->kv()[0];
    }

    function value()
    {
        return $this->kv()[1];
    }
}
