#!/bin/sh

a=`./noext.out noext noext.out noext.c`
test "$a" = 'noext
noext
noext'
