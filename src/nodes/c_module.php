<?php

class c_module
{
    public $elements = [];

    function format()
    {
        $s = '';
        foreach ($this->elements as $node) {
            $s .= $node->format();
        }
        return $s;
    }

    function json()
    {
        return ['type' => 'c_module', 'elements' => array_map(function ($element) {
            return $element->json();
        }, $this->elements)];
    }

    function imports()
    {
        $list = [];
        foreach ($this->elements as $element) {
            if ($element instanceof c_import) {
                $list[] = $element;
            }
        }
        return $list;
    }

    function types()
    {
        $list = [];
        foreach ($this->elements as $element) {
            if ($element instanceof c_typedef) {
                $list[] = $element->name();
            }
        }
        return $list;
    }

    function elements()
    {
        return $this->elements;
    }

    function merge(c_module $that)
    {
        $result = new self;
        $result->elements = array_merge($this->elements, $that->elements);
        return $result;
    }
}
