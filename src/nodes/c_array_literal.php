<?php

class c_array_literal_entry
{
    public $index = null;
    public $value;

    function format()
    {
        $s = '';
        if ($this->index) {
            $s .= '[' . $this->index->format() . '] = ';
        }
        $s .= $this->value->format();
        return $s;
    }
}

class c_array_literal
{
    public $values = [];

    function format()
    {
        $s = '{';
        foreach ($this->values as $i => $value) {
            if ($i > 0) $s .= ', ';
            $s .= $value->format();
        }
        if (empty($this->values)) {
            $s .= '0';
        }
        $s .= '}';
        return $s;
    }
}
