#!/bin/sh

export CHELANG_HOME=`pwd`
che=$CHELANG_HOME/target/debug/che

cd prog/$1
	name=`basename $1`
	$che build "$name.c" "$name.out"
	if [ $? = 0 ]; then
		echo OK build $name
	else
		fail "build $name"
		cd ..
		continue
	fi

	if [ -f 1.test ]; then
		sh 1.test > 1.output || exit 1
		diff 1.output 1.snapshot || exit 1
		echo OK "$1/1.test"
		rm 1.output
	fi

	if [ -f test.sh ]; then
		./test.sh || exit 1
		echo OK test $1
	fi
	rm *.out
	cd ..
cd ..
