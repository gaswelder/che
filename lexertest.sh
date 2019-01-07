# for f in `find . -name '*.c'`; do
#     echo $f
#     ./lexer < $f || break
# done

php lexertest.php < lexer.c > lexertest.php.txt
./che lexer.c || exit 1
./lexer < lexer.c > lexertest.che.txt
diff --suppress-common-lines lexertest.php.txt lexertest.che.txt
