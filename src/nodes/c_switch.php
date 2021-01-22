<?php

class c_switch
{
    public $value;
    public $cases = [];
    public $default;

    function format()
    {
        $s = '';
        foreach ($this->cases as $case) {
            $s .= $case->format() . "\n";
        }
        if ($this->default) {
            $s .= $this->default->format() . "\n";
        }
        return sprintf(
            "switch (%s) {\n%s\n}",
            $this->value->format(),
            indent($s)
        );
    }
}
