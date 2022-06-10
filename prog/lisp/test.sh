#!/bin/sh

a=`echo '(apply cons (quote (a (b c))))' | ./lisp.out`
test "$a" = '(a b c)'
