#!/bin/bash
#
# Regenerates all the files in the `autogen/` directory, to the degree
# that running `rm -rf autogen && ./regenerate.sh && make` would work.
#
# (The `autogen/` directory is not `.gitignore`-d to run the test on Windows too.)

mkdir -p autogen

cat <<EOF >autogen/current.h
#include "../../../current.h"
EOF

for SRC in gen_*.cc ; do
  BIN=.current/${SRC%.cc}
  make -s $BIN && $BIN
done

touch test.cc
