#!/bin/bash
#
# Consecutively runs `make test` from within each directory that contains a `test.cc` file.

set -u -e

DESIRED_SHARD=${1:-0}
DESIRED_MOD=${2:-1}

FAILURES=""

# A space-separated list of relative paths to exclude from the test run. Example: "./blocks/HTTP ./blocks/MMQ".
EXCLUDE=("")

for i in $(find . -name test.cc | sort) ; do
  DIR=$(dirname $i)
  MD5=$(echo $DIR | md5sum)
  HASH=$((0x${MD5%% *}))
  SHARD=$((((HASH % DESIRED_MOD) + DESIRED_MOD) % DESIRED_MOD))
  if [[ $DESIRED_MOD == 1 ]]; then
    echo -e "\n\033[0m\033[1mDir\033[0m: \033[36m${DIR}\033[0m"
  fi
  if [[ " ${EXCLUDE[@]} " =~ " $DIR " ]]; then
    echo "Excluded."
  elif [[ $SHARD == $DESIRED_SHARD ]]; then
    cd $DIR
    if ! CONTINUOUS_INTEGRATION=true make -s test ; then
      FAILURES="$FAILURES\n- $DIR"
    fi
    cd - >/dev/null
  fi
done

if [[ $DESIRED_SHARD == 0 ]]; then
  # And a dedicated run for the `dlopen()`-based compilation that involves a symlink to the very Current.
    echo -e "\n\033[0m\033[1mDir\033[0m: \033[36m./bricks/system\033[0m [with --current_base_dir_for_dlopen_test set]"
  ( \
    DIR=$(./scripts/fullpath.sh $PWD) && \
    cd bricks/system && \
    CONTINUOUS_INTEGRATION=true make .current/test > /dev/null \
    && ./.current/test --current_base_dir_for_dlopen_test=$DIR \
  ) || FAILURES="$FAILURES\n- bricks/system [with --current_base_dir_for_dlopen_test set]"
fi

if [ "$FAILURES" = "" ] ; then
  if [[ $DESIRED_MOD == 1 ]]; then
    echo -e "\n\033[32m\033[1mALL TESTS PASS.\033[0m"
  fi
  exit 0
else
 echo -e "\n\033[31m\033[1mFAIL\033[0m"
 echo -e "Failed tests:$FAILURES"
 exit 1
fi
