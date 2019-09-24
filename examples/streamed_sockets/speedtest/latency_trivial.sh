#!/bin/bash

trap "kill 0" EXIT

NDEBUG=1 make -j .current/forward .current/latency_trivial

./.current/forward &
./.current/latency_trivial &

wait
