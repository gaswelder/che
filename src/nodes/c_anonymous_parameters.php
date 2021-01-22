<?php

class c_anonymous_parameters
{
    public $forms = [];

    function format()
    {
        $s = '(';
        foreach ($this->forms as $i => $form) {
            if ($i > 0) $s .= ', ';
            $s .= $form->format();
        }
        $s .= ')';
        return $s;
    }
}
