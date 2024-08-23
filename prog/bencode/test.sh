#!/bin/sh

./bencode.out a.torrent > output.tmp
diff output output.tmp
r=$?
rm output.tmp
exit $r
