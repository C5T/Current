#!/bin/bash
#
# Run test for c5t-current-schema-ts.

set -eu

( cd ./c5t-current-schema-ts && npm install && npm prune && npm run --silent test )
