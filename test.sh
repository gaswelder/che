#!/bin/sh

# utils
cleardir () {
	if [ ! -d $1 ]; then return 0; fi
	n=`ls $1 | wc -l`
	if [ $n -gt 0 ]; then rm $1/*; fi
}

#
# build and use it
#
cargo build || exit 1
export CHELANG_HOME=`pwd`
che=$CHELANG_HOME/target/debug/che

#
# internal test
#
cargo test || exit 1

#
# scoped identifiers test
#
che build test/scoped/main.c && ./main || exit 1

#
# snapshots test
#
# tmpdir="tmp-snapshots"
# outpath=$tmpdir/cat
# mkdir $tmpdir
# for inpath in `ls test/snapshots/*.in.c`; do
# 	# translate the source
# 	cleardir $tmpdir
# 	che genc $inpath $tmpdir
# 	cat $tmpdir/* > $outpath

# 	# compare the output with the snapshot
# 	snappath=`echo $inpath | sed 's/\.in\./.out./'`
# 	diff $snappath $outpath
# 	if [ $? -eq 1 ]; then
# 		echo "mismatch in $inpath"
# 		exit 1
# 	else
# 		echo "OK: snapshot $inpath"
# 	fi
# done
# rm $tmpdir/*
# rmdir $tmpdir

#
# self-hosted lib tests
#
che test lib || exit 1


#
# ...
#
cd test
./tests.sh
cd ..

cd prog
	# Build all on top
	for i in *.c; do
		name=`basename $i .c`
		echo build $name
		$che build "$i" "$name.out" || exit 1
	done

	# Run all tests
	for i in *.test.sh; do
		name=`basename $i .test.sh`
		./$i && echo "OK $name" || exit 1
	done
	rm *.out
	rm -rf tmp

	# Build all in folders
	for i in */; do
		cd $i
		name=`basename $i`
		echo build "$name"
		$che build "$name.c" "$name.out" || exit 1
		if [ -f test.sh ]; then
			./test.sh || exit 1
			echo OK $i
		fi
		rm *.out
		cd ..
	done
cd ..

echo "all OK"