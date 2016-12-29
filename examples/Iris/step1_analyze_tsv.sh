#!/bin/sh
#
# Step 1: Analyze the TSV file, generate its description, JSON representation, and the schema header.

(cd data &&
 rm -f rm -f dataset.description dataset.json dataset.h &&
 ../../../Utils/JSONSchema/analyze_tsv.sh dataset.tsv) && echo OK
