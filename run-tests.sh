cargo test || exit $?

if [ ! -d bin ]; then
	mkdir bin || exit 1
fi

for i in prog/*.c; do
	name=`basename $i`
	echo $name
	cargo run build "$i" "bin/$name" || exit 1
done
