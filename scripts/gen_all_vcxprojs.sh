#!/usr/bin/env bash

CMD=$(./scripts/fullpath.sh ./scripts/gen_vcxproj.sh);
for i in $(find . -iname "*.cc" | xargs dirname | uniq | grep -v '/docu/' | grep -v '/3rdparty/'); do
  (cd "$i"; echo -n "$i: "; "$CMD")
done
