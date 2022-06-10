#!/bin/sh

cargo test || exit 1
cargo build || exit 1

export CHELANG_HOME=`pwd`

cd test
./tests.sh
cd ..

cd prog
for i in *.c; do
	name=`basename $i .c`
	echo $name
	../target/debug/che build "$i" "$name.out" || exit 1
	if [ -f "$name.test.sh" ]; then
		./$name.test.sh && echo "OK $name" || exit 1
	fi
done
rm *.out
cd ..

echo "all OK"