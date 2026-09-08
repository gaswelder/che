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
# self-hosted tests
#
che test lib || exit 1
che test samples || exit 1

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
