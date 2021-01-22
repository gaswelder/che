<?php

class c_union
{
    public $form;
    public $fields = [];

    function format()
    {
        $s = '';
        foreach ($this->fields as $field) {
            $s .= $field->format() . "\n";
        }
        return sprintf("union {\n%s\n} %s;\n", indent($s), $this->form->format());
    }
}
