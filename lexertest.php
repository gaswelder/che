<?php

require 'mc/parser/buf.php';
require 'mc/parser/token.php';
require 'mc/parser/lexer_1.php';

$src = '';
while (true) {
    $line = fgets(STDIN);
    if ($line === false) break;
    $src .= $line;
}

$s = new lexer_1($src);
while (true) {
    $tok = $s->get();
    if (!$tok) break;
    if ($tok->type == 'error') {
        throw new Exception("$tok->content at $tok->pos");
    }
    echo json_encode($tok), "\n";
}
