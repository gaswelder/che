php test.php || exit $?

if [ ! -d bin ]; then
	mkdir bin || exit 1
fi

for i in prog/*.c; do
	name=`basename $i`
	echo $name
	./che build "$i" "bin/$name" || exit 1
done

# ./che lexer < prog/lexer.c > lexertest.php.txt
# ./bin/lexer < prog/lexer.c > lexertest.che.txt
# diff --suppress-common-lines lexertest.php.txt lexertest.che.txt || echo "lexer outputs are different" && exit 1
