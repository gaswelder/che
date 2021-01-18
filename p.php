<?php

require 'src/buf.php';

$buf = new buf('123');

$v = call_rust('use_buf', $buf);
var_dump($v);

var_dump($buf->peek());
