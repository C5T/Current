#!/bin/bash
#
# This script illustrates how to use the CMakeLists.txt-based build of project that use `current`.
# It is meant to be run from within an empty directory.

set -e

touch .gitignore
touch golden_gitignore

if ! diff golden_gitignore .gitignore ; then
  echo "Diff failed on line ${LINENO}."
  exit 1
fi

if ! [ -s Makefile ] ; then
  echo 'Need the `Makefile`, `curl`-ing one.'
  curl -s https://raw.githubusercontent.com/dimacurrentai/Current/cmake/cmake/Makefile >Makefile
	# TODO(dkorolev): Fix the repo & branch on the line above once merged.
fi

mkdir -p src
cat >src/hw.cc <<EOF
#include <iostream>
int main() {
#ifdef NDEBUG
  std::cout << "Hello, World! NDEBUG=1." << std::endl;
#else
  std::cout << "Hello, World! NDEBUG is unset." << std::endl;
#endif
}
EOF

# This runs `cmake .` for Release mode, which is output into `.current`.
echo "::group::configure"
make .current
echo "::endgroup::"

# The run of `cmake .` must have cloned `current` and `googletest` and added them into `.gitignore`.
if ! [ -d current ] || ! [ -d googletest ] ; then
  echo 'Either `current` or `googletest` were not cloned.'
  exit 1
fi

echo 'current/' >>golden_gitignore
echo 'googletest/' >>golden_gitignore
if ! diff golden_gitignore .gitignore ; then
  echo "Diff failed on line ${LINENO}."
  exit 1
fi

echo "::group::make [release]"
make
echo "::endgroup::"

echo '.current/' >>golden_gitignore
if ! diff golden_gitignore .gitignore ; then
  echo "Diff failed on line ${LINENO}."
  exit 1
fi

echo "::group::.current/hw"
.current/hw
echo "::endgroup::"

echo "::group::make debug"
make debug
echo "::endgroup::"

echo '.current_debug/' >>golden_gitignore
diff golden_gitignore .gitignore || (echo 'Wrong `.gitignore`, exiting.'; exit 1)

echo "::group::.current_debug/hw"
.current_debug/hw
echo "::endgroup::"

cat >src/test_gtest.cc <<EOF
#include <gtest/gtest.h>  // IWYU pragma: keep
TEST(SmokeGoogletest, TwoTimesTwo) {
  EXPECT_EQ(4, 2 * 2);
}
EOF

cat >src/test_current_gtest.cc <<EOF
#include "3rdparty/gtest/gtest-main.h"  // IWYU pragma: keep
TEST(SmokeCurrentGoogletest, TwoTimesTwo) {
  EXPECT_EQ(4, 2 * 2);
}
EOF

echo "::group::release_test"
make test
echo "::endgroup::"

echo "::group::debug_test"
make debug_test
echo "::endgroup::"

touch src/test_gtest.cc
T0_GTEST=$(date +%s)
echo "::group::one line change google gtest release"
make test
echo "::endgroup::"
T1_GTEST=$(date +%s)

touch src/test_current_gtest.cc
T0_CURRENT_GTEST=$(date +%s)
echo "::group::one line change current gtest release"
make test
echo "::endgroup::"
T1_CURRENT_GTEST=$(date +%s)

touch src/test_gtest.cc
T0_DEBUG_GTEST=$(date +%s)
echo "::group::one line change google gtest debug"
make debug_test
echo "::endgroup::"
T1_DEBUG_GTEST=$(date +%s)

touch src/test_current_gtest.cc
T0_DEBUG_CURRENT_GTEST=$(date +%s)
echo "::group::one line change current gtest debug"
make debug_test
echo "::endgroup::"
T1_DEBUG_CURRENT_GTEST=$(date +%s)

echo "One-line change time, Current gtest, debug: $((T1_DEBUG_CURRENT_GTEST - T0_DEBUG_CURRENT_GTEST))s"
echo "One-line change time, Current gtest, release: $((T1_CURRENT_GTEST - T0_CURRENT_GTEST))s"
echo
echo "One-line change time, Google gtest, debug: $((T1_DEBUG_GTEST - T0_DEBUG_GTEST))s"
echo "One-line change time, Google gtest, release: $((T1_GTEST - T0_GTEST))s"
echo
echo '(The numbers for `Current gtest` should be worse, as Current is header-only.)'

# TODO(dkorolev): Add tests for `lib_*.cc` targets too, both as binaries and from tests.
