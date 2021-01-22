<?php

class c_function_parameter
{
    public $type;
    public $forms = [];

    function format()
    {
        $s = $this->type->format() . ' ';
        foreach ($this->forms as $i => $form) {
            if ($i > 0) $s .= ', ';
            $s .= $form->format();
        }
        return $s;
    }

    function translate()
    {
        $compat = [];
        foreach ($this->forms as $form) {
            $p = new self;
            $p->type = $this->type;
            $p->forms = [$form];
            $compat[] = $p;
        }
        return $compat;
    }
}
