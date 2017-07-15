#!/bin/bash
#
# Regenerates golden/* files from the test schema reflection.

set -eu

make clean && make .current/test && .current/test --write_reflection_golden_files && node ./indent-json.js
