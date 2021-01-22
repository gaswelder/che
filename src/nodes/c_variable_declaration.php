<?php

class c_variable_declaration
{
    public $type;
    public $forms = [];
    public $values = [];

    function format()
    {
        $s = $this->type->format() . ' ';
        foreach ($this->forms as $i => $form) {
            $value = $this->values[$i];
            if ($i > 0) $s .= ', ';
            $s .= $form->format() . ' = ' . $value->format();
        }
        $s .= ";\n";
        return $s;
    }
}
