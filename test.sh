if [ ! -d bin ]; then
	mkdir bin || exit 1
fi

cd bin
for i in ../test/*.c ../prog/*.c; do
	basename $i
	CHE_HOME=.. php ../che "$i" || exit 1
done
