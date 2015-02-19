#!/bin/bash
#
# Compiles all the tests into one binary, runs all the tests and generates the complete coverage report.
#
# Currently supports Linux only due to the coverage generation tool used.
#
# TODO(dkorolev): Look into making this script run on Mac.

set -u -e

CPPFLAGS="-std=c++11 -g -Wall -W -fprofile-arcs -ftest-coverage -DBRICKS_COVERAGE_REPORT_MODE"
LDFLAGS="-pthread"

TMPDIR=.noshit/fulltest
TMPDIR_FULL_PATH=$(readlink -e .)/.noshit/fulltest

GOLDEN_SUBDIR_NAME=golden
GOLDEN_FULL_PATH=$TMPDIR_FULL_PATH/$GOLDEN_SUBDIR_NAME

# Concatenate all `test.cc` files in the right way, creating one test to rule them all.
mkdir -p $TMPDIR
echo "// Magic. Watch." > $TMPDIR/ALL_TESTS_TOGETHER.cc

echo "#include \"dflags/dflags.h\"" >> $TMPDIR/ALL_TESTS_TOGETHER.cc
echo "#include \"3party/gtest/gtest-main-with-dflags.h\"" >> $TMPDIR/ALL_TESTS_TOGETHER.cc

echo -n -e "\033[0m\033[1mTests:\033[0m\033[36m"
for i in $(find . -iname "*test.cc" | grep -v 3party | grep -v "/.noshit/"); do
  echo "#include \"$i\"" >> $TMPDIR/ALL_TESTS_TOGETHER.cc
  echo -n " $i"
done

# Allow this one test to rule them all to access all the `golden/` files.
mkdir -p $GOLDEN_FULL_PATH
for dirname in $(find . -iname $GOLDEN_SUBDIR_NAME -type d | grep -v 3party | grep -v "/.noshit/"); do
  (cd $dirname; for filename in * ; do ln -sf $PWD/$filename $GOLDEN_FULL_PATH ; done)
done

(
  # Compile and run The Big Test.
  cd $TMPDIR

  echo -e "\033[0m"
  echo -n -e "\033[1mCompiling all tests together: \033[0m\033[31m"
  g++ $CPPFLAGS -I ../.. ALL_TESTS_TOGETHER.cc -o ALL_TESTS_TOGETHER $LDFLAGS
  echo -e "\033[32m\033[1mOK.\033[0m"

  echo -e "\033[1mRunning the tests and generating coverage info.\033[0m"
  ./ALL_TESTS_TOGETHER || exit 1
  echo -e "\033[32m\033[1mALL TESTS PASS.\033[0m"

  # Generate the resulting code coverage report.
  gcov ALL_TESTS_TOGETHER.cc >/dev/null
  geninfo . --output-file coverage0.info >/dev/null
  lcov -r coverage0.info /usr/include/\* \*/gtest/\* \*/3party/\* -o coverage.info >/dev/null
  genhtml coverage.info --output-directory coverage/ >/dev/null
  rm -rf coverage.info coverage0.info *.gcov *.gcda *.gcno
  echo
  echo -e -n "\033[0m\033[1mCoverage report\033[0m: \033[36m"
  readlink -e coverage/index.html
  echo -e -n "\033[0m"
)
