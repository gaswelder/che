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
	sampletest $n && echo OK sample $n || exit 1
done

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
