#!/bin/sh

export CHELANG_HOME=`pwd`
che=$CHELANG_HOME/target/debug/che

if [ "$1" = "" ]
then
	echo 'Arguments: <progname>'
	exit 1
fi

name=`basename $1`

(
	cd "$CHELANG_HOME/prog/$1" || exit 1

	$che build "$name.c" "$name.out" || {
		echo FAIL build $name
		exit 1
	}
	echo OK build $name

	if [ -f 1.test ]; then
		sh 1.test > 1.output || exit 1
		diff 1.output 1.snapshot || exit 1
		echo OK "$1/1.test"
		rm 1.output
	fi
	rm *.out
)
