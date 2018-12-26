for f in `find . -name '*.c'`; do
    echo $f
    ./lexer < $f || break
done
