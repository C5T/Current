#!/bin/bash
#
# Runs `check-headers.sh` in every subdirectory containing `*.h` files.

set -u -e

CURRENT_SCRIPTS_DIR=$( dirname "${BASH_SOURCE[0]}" )
CURRENT_SCRIPTS_DIR_FULL_PATH=$( "$CURRENT_SCRIPTS_DIR/fullpath.sh" "$CURRENT_SCRIPTS_DIR" )
RUN_DIR_FULL_PATH=$( "$CURRENT_SCRIPTS_DIR/fullpath.sh" "$PWD" )

echo -e "\033[1m\033[34mTesting all headers to comply with the header-only paradigm.\033[0m"

CURRENT_BUILD_FILE=""
# If `current_build.h` is present, we pass its full path as the first argument to `check-headers-within-dir.sh`.
if [[ -f "current_build.h" ]]; then
  CURRENT_BUILD_FILE="$RUN_DIR_FULL_PATH/current_build.h"
fi
for i in $(for i in $(find . -iname "*.h" | grep -v "/3rdparty/" | grep -vi deprecated | grep -v "/sandbox/" | grep -v "/golden/") ; do dirname "$i" ; done | sort -u) ; do
  echo -e "\033[1mDirectory\033[0m: $i"
  if [[ -n $CURRENT_BUILD_FILE ]]; then
    cd "$i" && eval "$CURRENT_SCRIPTS_DIR_FULL_PATH/check-headers-within-dir.sh -i $CURRENT_BUILD_FILE" && cd "$RUN_DIR_FULL_PATH" && continue
  else
    cd "$i" && "$CURRENT_SCRIPTS_DIR_FULL_PATH/check-headers-within-dir.sh" && cd "$RUN_DIR_FULL_PATH" && continue
  fi
  echo -e "\033[1m\033[31mTerminating.\033[0m" && exit 1
done

echo -e "\033[1m\033[32mConfirmed\033[34m:\033[1m All headers comply with the header-only paradigm.\033[0m"
