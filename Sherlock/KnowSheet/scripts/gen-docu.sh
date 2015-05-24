#!/bin/bash
#
# This script generates the documentation in GitHub markdown format.
# It scans all `docu_*.*` files, uses `.md` directly and takes the lines starting with double space from `.h`.
#
# Best practices:
#
# 1) Make `docu_*.cc` files that should be passing unit tests `docu_*_test.cc`.
#    This ensures they will be run by the full test make target, and thus it would
#    not so happen accidentally that some of the docu-mented code does not compile and/or pass tests.
#
# 2) Use header guards in those `docu_*_test.cc` files and `#include` them from the actual `test.cc`
#    for that Bricks module.
#    That would ensure that those `docu_*_test.cc` tests are run both by `make` from within that directory,
#    and by the top-level `make` that runs all tests at once and generates the coverage report.
#
# 3) Use the two spaces wisely.
#    If something should be in the docu, it should be indented.
#    If it should not be, it should not be indented.
#    The `docu_*.*` files are excluded from `make indent`.

set -u -e

for fn in $(for i in $(find . -type f -iname "docu_*.*" | grep -v ".noshit" | grep -v "zzz_full_test"); do
             echo -e "$(basename "$i")\t$i";
           done | sort | cut -f2) ; do
  echo $fn >/dev/stderr
  case $fn in
  *.cc)
    echo '```cpp'
    (grep '^  ' $fn || echo "  // No documentation data in '$fn'.") | sed "s/^  //g"
    echo '```'
    ;;
  *.md)
    cat $fn
    ;;
  *)
    echo "Unrecognized extension for file '$fn'."
    exit 1
    ;;
  esac
done
