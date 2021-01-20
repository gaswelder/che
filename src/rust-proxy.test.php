<?php

require_once 'src/buf.php';

test('buf instance', function () {
    $buf = new buf('123');
    eq('1', call_rust('call_buf_get', $buf));
    eq('2', $buf->get());
    eq('3', call_rust('call_buf_get', $buf));
});
