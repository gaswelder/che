#!/bin/sh

#
# internal test
#
cargo test || exit 1

#
# build and use the latest build
#
cargo build || exit 1
export CHELANG_HOME=`pwd`
che=$CHELANG_HOME/target/debug/che

#
# compile and test sample projects
#
sampletest () {
	name=$1
	che build samples/$name/main.c || return 1
	./main > tmp/output || return 1
	diff tmp/output samples/$name/out.snapshot || return 1
}
for n in `ls samples`; do
	sampletest $n
	if [ $? = 0 ]; then
		echo OK sample $n
	else
		echo FAIL sample $n
		exit 1
	fi
done

#
# self-hosted lib tests
#
che test lib || exit 1

fail () {
	echo FAIL $1
	if [ $BAIL != "" ]; then
		exit 1
	fi
}

#
# Build and test the world.
#
errors=0
cd prog
	for i in */; do
		cd $i
		name=`basename $i`
		$che build "$name.c" "$name.out"
		if [ $? = 0 ]; then
			echo OK build $name
		else
			fail "build $name"
			errors=`expr $errors + 1`
			cd ..
			continue
		fi

		if [ -f test.sh ]; then
			./test.sh || exit 1
			echo OK test $i
		fi
		rm *.out
		cd ..
	done
cd ..

if [ $errors = 0 ]; then
	echo "all OK"
else
	echo $errors failed to build
	exit $errors
fi

