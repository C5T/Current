#!/bin/bash
#
# Checks that every header can be compiled independently and that they do not violate ODR.
#
# Compiles each header with g++ and clang++, then links them all together to confirm that
# one-definition-rule is not violated, i.e., no non-inlined global symbols are defined.

set -u -e

CPPFLAGS="-std=c++11 -g -Wall -W -DBRICKS_CHECK_HEADERS_MODE"
LDFLAGS="-pthread"

if [ $(uname) = "Darwin" ] ; then
  CPPFLAGS+=" -stdlib=libc++ -x objective-c++ -fobjc-arc"
  LDFLAGS+=" -framework Foundation"
fi

# NOTE: TMP_DIR must be resolved from the current working directory.

TMP_DIR_NAME=".noshit"
TMP_STDOUT="$TMP_DIR_NAME/stdout.log"
TMP_STDERR="$TMP_DIR_NAME/stderr.log"

rm -rf "$TMP_DIR_NAME/headers"
mkdir -p "$TMP_DIR_NAME/headers"

echo -e -n "\033[1mCompiling\033[0m: "
for i in $(ls *.h | grep -v ".cc.h$") ; do
  echo -e -n "\033[36m"
  echo -n "$i "
  echo -e -n "\033[31m"
  ln -sf "$PWD/$i" "$PWD/$TMP_DIR_NAME/headers/$i.g++.cc"
  ln -sf "$PWD/$i" "$PWD/$TMP_DIR_NAME/headers/$i.clang++.cc"
  g++ -I . $CPPFLAGS \
    -c "$PWD/$TMP_DIR_NAME/headers/$i.g++.cc" \
    -o "$PWD/$TMP_DIR_NAME/headers/$i.g++.o" $LDFLAGS \
    >"$TMP_STDOUT" 2>"$TMP_STDERR" || (cat "$TMP_STDOUT" "$TMP_STDERR" && exit 1)
  clang++ -I . $CPPFLAGS \
    -c "$PWD/$TMP_DIR_NAME/headers/$i.clang++.cc" \
    -o "$PWD/$TMP_DIR_NAME/headers/$i.clang++.o" $LDFLAGS \
    >"$TMP_STDOUT" 2>"$TMP_STDERR" || (cat "$TMP_STDOUT" "$TMP_STDERR" && exit 1)
done
echo

echo -e -n "\033[0m\033[1mLinking\033[0m:\033[0m\033[31m "
echo -e '#include <cstdio>\nint main() { printf("OK\\n"); }\n' >"$TMP_DIR_NAME/headers/main.cc"
g++ -c $CPPFLAGS \
  -o "$TMP_DIR_NAME/headers/main.o" "$TMP_DIR_NAME/headers/main.cc" $LDFLAGS \
  >"$TMP_STDOUT" 2>"$TMP_STDERR" || (cat "$TMP_STDOUT" "$TMP_STDERR" && exit 1)
g++ -o "$TMP_DIR_NAME/headers/main" $TMP_DIR_NAME/headers/*.o $LDFLAGS \
  >"$TMP_STDOUT" 2>"$TMP_STDERR" || (cat "$TMP_STDOUT" "$TMP_STDERR" && exit 1)
echo -e -n "\033[1m\033[32m"
"$TMP_DIR_NAME/headers/main"

echo -e -n "\033[0m"
