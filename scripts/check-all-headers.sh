#!/bin/bash
#
# Runs `check-headers.sh` in every subdirectory containing `*.h` files.
#
# TODO(dkorolev): Move this `scripts/` directory into another repository.

set -u -e

DIR=$(dirname $(readlink -e "$BASH_SOURCE"))

echo -e "\033[1m\033[34mTesting all headers to comply with KnowSheet header-only paradigm.\033[0m"

for i in $(for i in $(find . -iname "*.h" | grep -v 3party) ; do dirname $i ; done | sort -u) ; do
  echo -e "\033[1mDirectory\033[0m: $i"
  if ! (cd $i; $DIR/check-headers.sh) ; then
    echo -e "\033[1m\033[31mTerminating.\033[0m"
    exit 1
  fi
done

echo -e "\033[1m\033[32mConfirmed\033[34m:\033[1m All headers comply with KnowSheet header-only paradigm.\033[0m"
