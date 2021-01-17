<?php

require __DIR__ . '/buf.php';

test('ended', function () {
    $buf = new buf('1');
    eq(false, $buf->ended());
    $buf->get();
    eq(true, $buf->ended());
});

test('pos', function () {
    $buf = new buf("123\n456");
    $seq = [
        "1:1",
        "1:2",
        "1:3",
        "1:4",
        "2:1",
        "2:2",
        "2:3"
    ];
    foreach ($seq as $pos) {
        eq($pos, $buf->pos());
        $buf->get();
    }
});

test('peek', function () {
    $buf = new buf('1');
    eq('1', $buf->peek());
    eq('1', $buf->peek());
    $buf->get();
    eq(null, $buf->peek());
});

test('get', function () {
    $buf = new buf('123');
    foreach (['1', '2', '3', null] as $expected) {
        eq($expected, $buf->get());
    }
});

test('unget', function () {
    $buf = new buf('123');
    eq('1', $buf->get());
    eq('2', $buf->get());
    $buf->unget('2');
    eq('2', $buf->get());
    eq('3', $buf->get());
    eq(null, $buf->get());
});

test('read_set', function () {
    $buf = new buf('aba123');;
    eq('', $buf->read_set('1234567890'));
    eq('aba', $buf->read_set('abcdef'));
    eq('1', $buf->get());
});

test('skip_literal', function () {
    $buf = new buf('abc123');
    eq(false, $buf->skip_literal('123'));
    eq(true, $buf->skip_literal('abc'));
    eq('1', $buf->get());
});

test('until_literal', function () {
    $buf = new buf('abc123');
    eq('abc', $buf->until_literal('123'));
    eq('1', $buf->get());
});

test('skip_until', function () {
    $buf = new buf('abc123');
    eq('abc', $buf->skip_until('1'));
    eq('123', $buf->skip_until('z'));
});
