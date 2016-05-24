#!/bin/bash
#
# Runs `check-headers.sh` in every subdirectory containing `*.h` files.

set -u -e

CURRENT_SCRIPTS_RELATIVE_DIR=$(dirname "${BASH_SOURCE[0]}")
CURRENT_SCRIPTS_DIR=$("$CURRENT_SCRIPTS_RELATIVE_DIR/fullpath.sh" "$CURRENT_SCRIPTS_RELATIVE_DIR")

CURRENT_BUILD=current_build.h.mock
RESOLVED_PWD=$(dirname "$("$CURRENT_SCRIPTS_DIR/fullpath.sh" "$PWD/$CURRENT_BUILD")")

echo -e "\033[1m\033[34mTesting all headers to comply with the header-only paradigm.\033[0m"

(export CPLUSPLUS=g++ ; export CPPFLAGS="" ; export LDFLAGS="" ; \
 "$CURRENT_SCRIPTS_DIR/gen-current-build.sh" "$CURRENT_BUILD" > /dev/null) \
|| (echo '`gen-current-build.sh` failed.' && exit 1)

for i in $(for i in $(find . -iname "*.h" | grep -v "/3rdparty/" | grep -vi "deprecated" | grep -v "/sandbox/" | grep -v "/golden/" | grep -v "$CURRENT_BUILD") ; do dirname "$i" ; done | sort -u) ; do
  echo -e "\033[1mDirectory\033[0m: $i"
  (cd "$i" && "$CURRENT_SCRIPTS_DIR/check-headers-within-dir.sh" "$RESOLVED_PWD") && continue
  echo -e "\033[1m\033[31mTerminating.\033[0m" && exit 1
done

rm -f "$CURRENT_BUILD"

echo -e "\033[1m\033[32mConfirmed\033[34m:\033[1m All headers comply with the header-only paradigm.\033[0m"
