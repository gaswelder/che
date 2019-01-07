if [ ! -d bin ]; then
	mkdir bin || exit 1
fi

cd bin

for i in ../test/*.c ../prog/*; do
	basename $i
	CHE_HOME=.. php ../che "$i" || exit 1
done

cd ..

./che lexer < prog/lexer.c > lexertest.php.txt
./bin/lexer < prog/lexer.c > lexertest.che.txt
diff --suppress-common-lines lexertest.php.txt lexertest.che.txt || echo "lexer outputs are different" && exit 1
