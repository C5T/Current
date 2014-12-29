#!/bin/bash
#
# Runs `check-headers.sh` in every subdirectory containing `*.h` files.
#
# TODO(dkorolev): Move this `scripts/` directory into another repository.

set -u -e

SCRIPT_DIR=$(dirname $(readlink -e "$BASH_SOURCE"))
RUN_DIR=$PWD

echo -e "\033[1m\033[34mTesting all headers to comply with the header-only paradigm.\033[0m"

for i in $(for i in $(find . -iname "*.h" | grep -v 3party) ; do dirname $i ; done | sort -u) ; do
  echo -e "\033[1mDirectory\033[0m: $i"
  cd $i && $SCRIPT_DIR/check-headers.sh && cd $RUN_DIR && continue
  echo -e "\033[1m\033[31mTerminating.\033[0m" && exit 1
done

echo -e "\033[1m\033[32mConfirmed\033[34m:\033[1m All headers comply with the header-only paradigm.\033[0m"
