<?php

function module_imports(c_module $module)
{
    $list = [];
    foreach ($module->elements as $element) {
        if ($element instanceof c_import) {
            $list[] = $element;
        }
    }
    return $list;
}

function module_types(c_module $module)
{
    $list = [];
    foreach ($module->elements as $element) {
        if (is_array($element) && $element['kind'] == 'c_typedef') {
            $list[] = format_node($element['alias']);
        }
    }
    return $list;
}

function merge_modules(c_module $a, c_module $b)
{
    $result = new c_module;
    $result->elements = array_merge($a->elements, $b->elements);
    return $result;
}
