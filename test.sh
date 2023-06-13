#!/bin/sh

# utils
cleardir () {
	if [ ! -d $1 ]; then return 0; fi
	n=`ls $1 | wc -l`
	if [ $n -gt 0 ]; then rm $1/*; fi
}

#
# build and use it
#
cargo build || exit 1
export CHELANG_HOME=`pwd`
che=$CHELANG_HOME/target/debug/che

#
# internal test
#
cargo test || exit 1

#
# scoped identifiers test
#
che build test/scoped/main.c && ./main || exit 1

#
# self-hosted lib tests
#
che test lib || exit 1


#
# ...
#
cd test
./tests.sh
cd ..

#
# Build and test the world.
#
cd prog
	for i in */; do
		cd $i
		name=`basename $i`
		$che build "$name.c" "$name.out" || exit 1
		echo OK build "$name"
		if [ -f test.sh ]; then
			./test.sh || exit 1
			echo OK test $i
		fi
		rm *.out
		cd ..
	done
cd ..

echo "all OK"
