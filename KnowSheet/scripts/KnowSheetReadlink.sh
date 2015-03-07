#!/bin/bash
#
# `readlink -e` replacement compatible with Mac OS X `readlink` implementation.
# Credits to http://stackoverflow.com/questions/59895/can-a-bash-script-tell-what-directory-its-stored-in and https://github.com/tarruda/zsh-autosuggestions/issues/38#issuecomment-69258372

set -u -e

SOURCE="$1"
while [ -h "$SOURCE" ]; do  # Resolve $SOURCE until the file is no longer a symlink.
  DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ "$SOURCE" != /* ]] && SOURCE="$DIR/$SOURCE"  # If $SOURCE was a relative symlink, we need to resolve it 
                                                  # relative to the path where the symlink file was located.
done
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

echo "$DIR/$(basename "$1")"
