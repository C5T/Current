#!/bin/bash
#
# Generates code coverage report using gcov/geninfo/genhtml for each .cc file in current directory.

set -u -e

CPPFLAGS="-std=c++11 -g -Wall -W -fprofile-arcs -ftest-coverage -DBRICKS_COVERAGE_REPORT_MODE"
LDFLAGS="-pthread"

TMPDIR=.noshit

mkdir -p $TMPDIR

for i in *.cc ; do
  echo -e "\033[0m\033[1m$i\033[0m: \033[33mGenerating coverage report.\033[0m"
  BINARY=${i%".cc"}
  rm -rf $TMPDIR/coverage/$BINARY
  mkdir -p $TMPDIR/coverage/$BINARY
  g++ $CPPFLAGS $i -o $TMPDIR/coverage/$BINARY/binary $LDFLAGS
  ./$TMPDIR/coverage/$BINARY/binary || exit 1
  gcov $i >/dev/null
  geninfo . --output-file coverage0.info >/dev/null
  lcov -r coverage0.info /usr/include/\* \*/gtest/\* \*/3party/\* -o coverage.info >/dev/null
  genhtml coverage.info --output-directory $TMPDIR/coverage/$BINARY >/dev/null
  rm -rf coverage.info coverage0.info *.gcov *.gcda *.gcno
  echo -e -n "\033[0m\033[1m$i\033[0m: \033[36m"
  echo $(readlink -e $TMPDIR/coverage/$BINARY/index.html)
  echo -e -n "\033[0m"
done
