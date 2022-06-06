#!/bin/sh

cargo test || exit $?

if [ ! -d bin ]; then
	mkdir bin || exit 1
fi

cargo build || exit 1

for i in prog/*.c; do
	name=`basename $i .c`
	echo $name
	target/debug/che build "$i" "prog/$name.out" || exit 1
	if [ -f "prog/$name.test.sh" ]; then
		cd prog
		./$name.test.sh && echo "OK $name" || exit 1
		cd ..
	fi
done
rm prog/*.out

for i in test/*.c; do
	name=`basename $i .c`
	echo $name
	target/debug/che build "$i" "test/$name.out" || exit 1
	if [ -f "test/$name.test.sh" ]; then
		cd test
		./$name.test.sh && echo "OK $name" || exit 1
		cd ..
	fi
done
rm test/*.out