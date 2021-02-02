<?php

function module_imports($module)
{
    $list = [];
    foreach ($module['elements'] as $element) {
        if (is_array($element) && $element['kind'] === 'c_import') {
            $list[] = $element;
        }
    }
    return $list;
}

function module_types($module)
{
    $list = [];
    foreach ($module['elements'] as $element) {
        if (is_array($element) && $element['kind'] == 'c_typedef') {
            $list[] = format_node($element['alias']);
        }
    }
    return $list;
}
