<?php

function get_module($name)
{
    return call_rust('get_module', $name);
}

function operator_strength($op)
{
    return call_rust_mem("operator_strength", $op);
}
