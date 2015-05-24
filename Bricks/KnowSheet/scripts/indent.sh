#!/bin/bash
#
# Indents all *.cc and *.h files in the current directory and below
# according to .clang-format in this directory or above.

(find . -name "*.cc" ; find . -name "*.h") \
| grep -v "/.noshit/" | grep -v "/3party/" \
| grep -v "/docu_" \
| xargs clang-format-3.5 -i
