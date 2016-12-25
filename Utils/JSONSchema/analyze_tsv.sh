#!/bin/bash

# Takes a dense `*.tsv` file as an input, and generates the corresponding `.json`, `.description`, and `.h` files.

set -u -e
SCRIPT_DIR="$(dirname "${BASH_SOURCE[0]}")"

if [ $# -lt 1 ] ; then
  echo "Synopsis: ./analyze_tsv.h input.tsv [extension, 'tsv' by default] [fields separator, '\\t' by default]"
  exit 1
fi

FILE_ORIGINAL=$1
EXTENSION=${2:-tsv}
FILE_JSON="${FILE_ORIGINAL/\.$EXTENSION/\.json}"
FILE_DESCRIPTION="${FILE_ORIGINAL/\.$EXTENSION/\.description}"
FILE_H="${FILE_ORIGINAL/\.$EXTENSION/\.h}"

if [[ ! "$FILE_ORIGINAL" =~ .*\.$EXTENSION$ ]]; then
  echo "Unrecognized extension, if not 'tsv', pass it as the second parameter."
  exit 1
fi

if [ -e "$FILE_JSON" ] ; then
  echo "'$FILE_JSON' exists, aborting."
  exit 1
fi

if [ -e "$FILE_DESCRIPTION" ] ; then
  echo "'$FILE_DESCRIPTION' exists, aborting."
  exit 1
fi

if [ -e "$FILE_H" ] ; then
  echo "'$FILE_H' exists, aborting."
  exit 1
fi

echo -n "Generating $FILE_JSON        ..."
if [ $# -lt 3 ] ; then
  cat "$FILE_ORIGINAL" | "$SCRIPT_DIR/.current/tsv2json" --header > "$FILE_JSON"
else
  SEPARATOR="$3"
  cat "$FILE_ORIGINAL" | "$SCRIPT_DIR/.current/tsv2json" --separator "$SEPARATOR"  --header > "$FILE_JSON"
fi
echo -e "\b\b\b: Done."

echo -n "Generating $FILE_DESCRIPTION ..."
"$SCRIPT_DIR/.current/describe" --input "${FILE_ORIGINAL/\.$EXTENSION/\.json}" --output "$FILE_DESCRIPTION"
echo -e "\b\b\b: Done."

echo -n "Generating $FILE_H           ..."
"$SCRIPT_DIR/.current/json2current" --input "${FILE_ORIGINAL/\.$EXTENSION/\.json}" --output "$FILE_H"
echo -e "\b\b\b: Done."
