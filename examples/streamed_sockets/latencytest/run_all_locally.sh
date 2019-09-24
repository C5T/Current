#!/bin/bash

trap "kill 0" EXIT

NDEBUG=1 make -j .current/generator .current/indexer .current/forward .current/processor .current/terminator

./.current/terminator --silent &
./.current/processor &
./.current/forward &
./.current/indexer &
./.current/generator &

wait
