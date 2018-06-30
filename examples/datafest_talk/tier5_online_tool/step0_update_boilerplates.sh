#!/bin/bash

# To run: ./step0_update_boilerplates.sh > inl_files.h
#
# Regenerates the C++ header containing the base64-encoded contents of all `.inl` files.

echo "#pragma once"
echo
echo "#include <string>"
echo
echo "#include \"../../../bricks/util/base64.h\""
echo
echo "// clang-format off"
for i in *.inl ; do
  echo -n "static const std::string static_file_$(echo $i | sed 's/\./_/g') = current::Base64Decode(\""
  base64 -w 0 $i
  echo "\");"
done
echo "// clang-format on"
