#!/bin/sh
Q=/home/claude/quickjs; SRC="$Q/quickjs.c $Q/dtoa.c $Q/libregexp.c $Q/libunicode.c $Q/cutils.c"; V=$(cat $Q/VERSION)
for arch in 64 32; do
  gcc -m$arch -O2 -D_GNU_SOURCE -DCONFIG_VERSION=\"$V\" -I$Q -o host$arch host.c $SRC -lm -lpthread
done
