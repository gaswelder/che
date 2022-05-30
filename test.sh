#!/bin/sh

cargo test || exit $?

if [ ! -d bin ]; then
	mkdir bin || exit 1
fi

cargo build || exit 1

for i in prog/*.c; do
	name=`basename $i .c`
	echo $name
	target/debug/che build "$i" "prog/$name.out"
	if [ -f "prog/$name.test.sh" ]; then
		cd prog
		./$name.test.sh && echo "OK $name" || exit 1
		cd ..
	fi
done

rm prog/*.out
