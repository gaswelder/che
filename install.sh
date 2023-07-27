#!/bin/sh

name=$1

che build prog/$name/$name.c || exit 1
mv $name ~/bin
