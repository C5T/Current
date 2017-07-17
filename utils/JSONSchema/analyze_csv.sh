#!/bin/bash

# Takes a dense `*.csv` file as an input, and generates the corresponding `.json`, `.description`, and `.h` files.

set -u -e
SCRIPT_DIR="$(dirname "${BASH_SOURCE[0]}")"

if [ $# -lt 1 ] ; then
  echo "Synopsis: ./analyze_csv.h input.csv"
  exit 1
fi

"$SCRIPT_DIR/analyze_tsv.sh" "$1" csv ,
