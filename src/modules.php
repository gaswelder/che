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
