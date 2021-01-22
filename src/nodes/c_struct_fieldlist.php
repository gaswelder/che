<?php

class c_struct_fieldlist
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
        $s .= ';';
        return $s;
    }
}
