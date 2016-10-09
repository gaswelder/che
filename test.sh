cd bin
for i in ../test/*.c ../prog/*.c; do
	basename $i
	CHE_HOME=../ ../che $i || exit 1
done
