#!/bin/bash
#
# Goes up the directory tree until "KnowSheet/" sub-directory is found.
# Prints its path.

dir=$(cd $(dirname $0) && pwd)
subdir="KnowSheet"

while [ ! -d "${dir}/${subdir}" ] ; do
  [ "$dir" == "/" ] && break
  dir=$(dirname "$dir")
done

[ -d "${dir}/${subdir}" ] && echo "${dir}/${subdir}" || exit 1
