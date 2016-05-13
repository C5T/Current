#!/bin/bash
#
# Checks that every header can be compiled independently and that they do not violate ODR.
#
# Compiles each header with g++ and clang++, then links them all together to confirm that
# one-definition-rule is not violated, i.e., no non-inlined global symbols are defined.

set -u -e

EXTRA_INCLUDE_DIR="${1:-.}"

CPPFLAGS="-std=c++11 -g -Wall -W -DCURRENT_MAKE_CHECK_MODE"
LDFLAGS="-pthread"

if [ $(uname) = "Darwin" ] ; then
  CPPFLAGS+=" -stdlib=libc++ -x objective-c++ -fobjc-arc"
  LDFLAGS+=" -framework Foundation"
fi

# NOTE: TMP_DIR must be resolved from the current working directory.

SOURCE_FILE="$PWD/.current_check_headers"
TMP_STDOUT="$PWD/.current_stdout.txt"
TMP_STDERR="$PWD/.current_stderr.txt"

rm -f "$PWD"/.current_*.cc "$PWD"/.current_*.o "$PWD"/.current_.txt "$SOURCE_FILE"

echo -e -n "\033[1mCompiling\033[0m: "
for i in $(ls *.h | grep -v ".cc.h$" | grep -v "^current_build.mock.h$") ; do
  echo -e -n "\033[36m"
  echo -n "$i "
  echo -e -n "\033[31m"
  HEADER_GCC="$PWD/.current_$i.g++.cc"
  HEADER_CLANG="$PWD/.current_$i.clang++.cc"
  COMBINED_OBJECT="$PWD/.current_${i}_combined"

  ln -sf "$PWD/$i" "$HEADER_GCC"
  ln -sf "$PWD/$i" "$HEADER_CLANG"

  g++ -I "$EXTRA_INCLUDE_DIR" $CPPFLAGS -c "$HEADER_GCC" -o "${HEADER_GCC}.o" $LDFLAGS \
    >"$TMP_STDOUT" 2>"$TMP_STDERR" || (cat "$TMP_STDOUT" "$TMP_STDERR" && exit 1)
  clang++ -I "$EXTRA_INCLUDE_DIR" $CPPFLAGS -c "$HEADER_CLANG" -o "${HEADER_CLANG}.o" $LDFLAGS \
    >"$TMP_STDOUT" 2>"$TMP_STDERR" || (cat "$TMP_STDOUT" "$TMP_STDERR" && exit 1)
  ld -r "${HEADER_GCC}.o" "${HEADER_CLANG}.o" -o "${COMBINED_OBJECT}.o"
done
echo

echo -e -n "\033[0m\033[1mLinking\033[0m:\033[0m\033[31m "
echo -e '#include <cstdio>\nint main() { printf("OK\\n"); }\n' >"${SOURCE_FILE}.cc"
g++ -I "$EXTRA_INCLUDE_DIR" -c $CPPFLAGS -o "${SOURCE_FILE}.o" "${SOURCE_FILE}.cc" $LDFLAGS \
  >"$TMP_STDOUT" 2>"$TMP_STDERR" || (cat "$TMP_STDOUT" "$TMP_STDERR" && exit 1)
g++ -I "$EXTRA_INCLUDE_DIR" -o "$SOURCE_FILE" "$PWD"/.current_*.o $LDFLAGS \
  >"$TMP_STDOUT" 2>"$TMP_STDERR" || (cat "$TMP_STDOUT" "$TMP_STDERR" && exit 1)
echo -e -n "\033[1m\033[32m"

"$SOURCE_FILE"
rm -f "$PWD"/.current_*.cc "$PWD"/.current_*.o "$PWD"/.current_*.txt $SOURCE_FILE

echo -e -n "\033[0m"
