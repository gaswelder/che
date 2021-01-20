#!/bin/sh

/usr/bin/time -f "%x\t%M\t%S\t%U" -a -o "times.txt" ./run-tests.sh
