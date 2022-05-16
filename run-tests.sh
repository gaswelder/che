cargo test || exit $?

if [ ! -d bin ]; then
	mkdir bin || exit 1
fi

cargo build || exit 1

for i in prog/*.c; do
	name=`basename $i`
	echo $name
	target/debug/che build "$i" "bin/$name" || exit 1
done
