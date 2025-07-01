#!/bin/sh

cat in.txt | ./cue-from-text.out "A Mind Confused" "A Mind Confused - A Mind Confused (1995).mp3" > /tmp/a
diff /tmp/a out.txt
