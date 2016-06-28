#!/bin/bash
#
# Regenerates all the files in the `golden/` directory, to the degree
# that running `rm -rf golden && ./regenerate.sh && make` would work.
#
# (The `golden/` directory is not `.gitignore`-d to run the test on Windows too.)

mkdir -p golden

cat <<EOF >golden/current.h
#include "../../../current.h"
EOF

for SRC in gen_*.cc ; do
  BIN=.current/${SRC%.cc}
  make -s $BIN && $BIN
done

touch test.cc
