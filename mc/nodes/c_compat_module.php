<?php

class c_compat_module
{
    private $elements = [];
    private $link = [];

    function __construct($elements, $link)
    {
        $this->elements = $elements;
        $this->link = $link;
    }

    function format()
    {
        $s = '';
        foreach ($this->elements as $node) {
            $s .= $node->format();
        }
        return $s;
    }

    function link()
    {
        return $this->link;
    }

    function synopsis()
    {
        $elements = [];

        foreach ($this->elements as $element) {
            if ($element instanceof c_compat_function_declaration && !$element->is_static()) {
                $elements[] = $element->forward_declaration();
                continue;
            }
            if ($element instanceof c_typedef) {
                $elements[] = $element;
                continue;
            }
            if ($element instanceof c_struct_definition) {
                $elements[] = $element;
                continue;
            }
            if ($element instanceof c_compat_macro) {
                $elements[] = $element;
                continue;
            }
            if ($element instanceof c_compat_enum && !$element->is_private()) {
                $elements[] = $element;
                continue;
            }
        }
        return $elements;
    }
}
