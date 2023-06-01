#!/bin/sh

# scoped identifiers test
cargo run build test/scoped/main.c && ./main || exit 1


# all the rest
export CHELANG_HOME=`pwd`
che=$CHELANG_HOME/target/debug/che

cargo test || exit 1
cargo build || exit 1
che test lib || exit 1


cd test
./tests.sh
cd ..

cd prog
	# Build all on top
	for i in *.c; do
		name=`basename $i .c`
		echo build $name
		$che build "$i" "$name.out" || exit 1
	done

	# Run all tests
	for i in *.test.sh; do
		name=`basename $i .test.sh`
		./$i && echo "OK $name" || exit 1
	done
	rm *.out
	rm -rf tmp

	# Build all in folders
	for i in */; do
		cd $i
		name=`basename $i`
		echo build "$name"
		$che build "$name.c" "$name.out" || exit 1
		if [ -f test.sh ]; then
			./test.sh || exit 1
			echo OK $i
		fi
		rm *.out
		cd ..
	done
cd ..

echo "all OK"