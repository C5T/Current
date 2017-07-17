#!/bin/bash

# Runs storage performance tests in bulk.

if [ ! -f .current/run ] ; then
  echo "Building '.current/run' to run the tests. You may want to check the compilation flags."
  make .current/run
fi

CMD="./.current/run --scenario=storage"

for STORAGE_TRANSACTION in empty size get put ; do
  for STORAGE_TEST_STRING in false true ; do
    for STORAGE_INITIAL_SIZE in 1000 50000 1000000 ; do
      for TEST_SECONDS in 2 ; do
        echo -n $STORAGE_TRANSACTION
        [ $STORAGE_TEST_STRING == true ] && echo -n ",string" || echo -n ",int"
        echo -n ",size=$STORAGE_INITIAL_SIZE : "
        $CMD \
          --storage_transaction=$STORAGE_TRANSACTION \
          --storage_initial_size=$STORAGE_INITIAL_SIZE \
          --storage_test_string=$STORAGE_TEST_STRING \
          --seconds=$TEST_SECONDS
      done
    done
  done
done
