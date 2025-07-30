#!/bin/sh

ffmpeg -framerate 10 -pattern_type sequence -i dump/out-%010d.bmp -s:v 1000x1000 -c:v libx264 -pix_fmt yuv420p out.mp4
