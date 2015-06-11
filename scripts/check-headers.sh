#!/bin/bash
#
# Runs `check-headers.sh` in every subdirectory containing `*.h` files.

set -u -e

CURRENT_SCRIPTS_DIR=$( dirname "${BASH_SOURCE[0]}" )
CURRENT_SCRIPTS_DIR_FULL_PATH=$( "$CURRENT_SCRIPTS_DIR/fullpath.sh" "$CURRENT_SCRIPTS_DIR" )
RUN_DIR_FULL_PATH=$( "$CURRENT_SCRIPTS_DIR/fullpath.sh" "$PWD" )

echo -e "\033[1m\033[34mTesting all headers to comply with the header-only paradigm.\033[0m"

for i in $(for i in $(find . -iname "*.h" | grep -v 3rdparty | grep -vi deprecated) ; do dirname "$i" ; done | sort -u) ; do
  echo -e "\033[1mDirectory\033[0m: $i"
  cd "$i" && "$CURRENT_SCRIPTS_DIR_FULL_PATH/check-headers-within-dir.sh" && cd "$RUN_DIR_FULL_PATH" && continue
  echo -e "\033[1m\033[31mTerminating.\033[0m" && exit 1
done

echo -e "\033[1m\033[32mConfirmed\033[34m:\033[1m All headers comply with the header-only paradigm.\033[0m"
