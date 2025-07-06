#!/bin/sh
src=$1
shift
che build "$src" /tmp/chebuild && /tmp/chebuild "$@"
