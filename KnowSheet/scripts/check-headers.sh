#!/bin/bash
#
# Checks that every header can be compiled independently and that they do not violate ODR.
#
# Compiles each header with g++ and clang++, then links them all together to confirm that
# one-definition-rule is not violated, i.e., no non-inlined global symbols are defined.

set -u -e

CPPFLAGS="-std=c++11 -g -Wall -W -DBRICKS_CHECK_HEADERS_MODE"
LDFLAGS="-pthread"

TMPDIR=.noshit

rm -rf $TMPDIR/headers
mkdir -p $TMPDIR/headers

echo -e -n "\033[1mCompiling\033[0m: "
for i in $(ls *.h | grep -v ".cc.h$") ; do
  echo -e -n "\033[36m"
  echo -n "$i "
  echo -e -n "\033[31m"
  ln -sf $PWD/$i $PWD/$TMPDIR/headers/$i.g++.cc
  ln -sf $PWD/$i $PWD/$TMPDIR/headers/$i.clang++.cc
  g++ -I . $CPPFLAGS -c $PWD/$TMPDIR/headers/$i.g++.cc -o $PWD/$TMPDIR/headers/$i.g++.o $LDFLAGS
  clang++ -I . $CPPFLAGS -c $PWD/$TMPDIR/headers/$i.clang++.cc -o $PWD/$TMPDIR/headers/$i.clang++.o $LDFLAGS
done
echo

echo -e -n "\033[0m\033[1mLinking\033[0m:\033[0m\033[31m "
echo -e '#include <cstdio>\nint main() { printf("OK\\n"); }\n' >$TMPDIR/headers/main.cc
g++ -c $CPPFLAGS -o $TMPDIR/headers/main.o $TMPDIR/headers/main.cc $LDFLAGS
g++ -o $TMPDIR/headers/main $TMPDIR/headers/*.o $LDFLAGS
echo -e -n "\033[1m\033[32m"
$TMPDIR/headers/main

echo -e -n "\033[0m"
