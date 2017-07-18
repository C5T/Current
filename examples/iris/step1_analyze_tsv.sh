#!/bin/sh
#
# Step 1: Analyze the TSV file, generate its description, JSON representation, and the schema header.

(cd data &&
 rm -f dataset.description dataset.json dataset.h &&
 ../../../utils/json_schema/analyze_tsv.sh dataset.tsv) && echo OK
