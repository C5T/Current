#!/bin/bash

# Regenerate `README.md` in the current directory using `gen_docu.sh` from this directory
# if the `docu/` subdirectory in the current directory does exist.

set -u -e

CURRENT_SCRIPTS_RELATIVE_DIR=$(dirname "${BASH_SOURCE[0]}")

if [ -d docu ] ; then "$CURRENT_SCRIPTS_RELATIVE_DIR/gen-docu.sh" > README.md ; fi
