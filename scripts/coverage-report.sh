#!/bin/bash
#
# Generates code coverage report using gcov/geninfo/genhtml for each .cc file in current directory.

set -u -e

CPPFLAGS="-std=c++11 -g -Wall -W -fprofile-arcs -ftest-coverage -DBRICKS_COVERAGE_REPORT_MODE"
LDFLAGS="-pthread"

# NOTE: TMP_DIR must be resolved from the current working directory.

CURRENT_SCRIPTS_DIR=$( dirname "${BASH_SOURCE[0]}" )
RUN_DIR_FULL_PATH=$( "$CURRENT_SCRIPTS_DIR/fullpath.sh" "$PWD" )

TMP_DIR_NAME=".current"
TMP_DIR_FULL_PATH="$RUN_DIR_FULL_PATH/.current"

mkdir -p "$TMP_DIR_NAME"

for i in *.cc ; do
  echo -e "\033[0m\033[1m$i\033[0m: \033[33mGenerating coverage report.\033[0m"
  BINARY="${i%".cc"}"
  rm -rf "$TMP_DIR_NAME/coverage/$BINARY"
  mkdir -p "$TMP_DIR_NAME/coverage/$BINARY"
  g++ $CPPFLAGS "$i" -o "$TMP_DIR_NAME/coverage/$BINARY/binary" $LDFLAGS
  "./$TMP_DIR_NAME/coverage/$BINARY/binary" || exit 1
  gcov "$i" >/dev/null
  geninfo . --output-file coverage0.info >/dev/null
  lcov -r coverage0.info /usr/include/\* \*/gtest/\* \*/3rdparty/\* -o coverage.info >/dev/null
  genhtml coverage.info --output-directory "$TMP_DIR_NAME/coverage/$BINARY" >/dev/null
  rm -rf coverage.info coverage0.info *.gcov *.gcda *.gcno
  echo -e -n "\033[0m\033[1m$i\033[0m: \033[36m"
  echo -n "$TMP_DIR_FULL_PATH/coverage/$BINARY/index.html"
  echo -e -n "\033[0m"
done
