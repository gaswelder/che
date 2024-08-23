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
# Run self-hosted lib tests.
#
che test lib || exit 1

#
# Build and test the world.
#
errors=0
for p in `ls prog`; do
	./progtest.sh $p
	if [ $? != 0 ]; then
		errors=`expr $errors + 1`
	fi
done

#
# Summary.
#
if [ $errors = 0 ]; then
	echo "all OK"
else
	echo $errors errors in prog/
	exit $errors
fi
