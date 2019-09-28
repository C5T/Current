#!/bin/bash

trap "kill 0" EXIT

NDEBUG=1 make -j .current/generator .current/indexer .current/forward .current/processor .current/terminator || exit 1

# A flag to set locally is `--skip_fwrite`.
./.current/terminator --silent &
./.current/processor &
./.current/forward $* &
./.current/indexer $* &

sleep 0.5
GENERATOR_FLAGS=""
GENERATOR_FLAGS="$GENERATOR_FLAGS --evensodds"  # Full output, to compare latencies on 1st and last blobs per block sent.
./.current/generator $GENERATOR_FLAGS &

wait
