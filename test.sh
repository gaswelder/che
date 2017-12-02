if [ ! -d bin ]; then
	mkdir bin || exit 1
fi

cd bin

for i in ../test/*.c ../prog/*.c; do
	basename $i
	CHE_HOME=.. php ../che "$i" || exit 1
done

for i in ../prog/*; do
	if [ ! -d "$i" ]; then continue; fi
	name=`basename "$i"`
	echo $name
	CHE_HOME=.. php ../che "$i/$name.c" || exit 1
done
