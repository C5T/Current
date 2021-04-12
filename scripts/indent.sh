#!/bin/bash
#
# Indents all *.cc and *.h files in the current directory and below
# according to .clang-format in this directory or above.

CLANG_FORMAT=${1:-clang-format-10}
(find . -type f -name "*.cc" ; find . -type f -name "*.h") \
| grep -v "/.current/" | grep -v "/3rdparty/" \
| grep -v "/docu_" \
| xargs "$CLANG_FORMAT" -i
