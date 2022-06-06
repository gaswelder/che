#!/bin/sh

a=`./exec.out exec.c`
test "$a" = 'exec.c
err = (null), exit status = 0'
