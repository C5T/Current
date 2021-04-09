#!/usr/bin/env bash

CMD=$(./scripts/fullpath.sh ./scripts/gen_vcxproj.sh); for i in $(find . -iname "*.cc" | xargs dirname | uniq); do (cd "$i"; "$CMD"; echo "$i"); done
