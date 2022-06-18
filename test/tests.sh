#!/bin/sh

run () {
    src=$1
    out=`basename $src .c`.out
    che build $src $out
    actual=$(./$out $2)
    s=$?
    if [ $s != 0 ]; then
        echo "$out returned status $s"
        exit 1
    fi
    expected="$3"
    if [ "$actual" != "$expected" ]; then
        echo FAIL in $1
        echo expected
        echo "$expected"
        echo got
        echo "$actual"
        exit 1
    fi
    echo OK $src
}

run arr.c '.' 'OK'
run exec.c 'exec.c' 'exec.c'
run xml.c '.' "dir dir1
  file file1
  file file2
dir dir3"

rm *.out
