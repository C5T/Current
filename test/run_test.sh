#!/bin/bash
for function in $(ls functions/ | cut -f1 -d.) ; do 
  echo "Function: "$function
  for action in generate evaluate intermediate_evaluate ; do
    echo -n "  "$action" "
    ./test_binary $function $action 3
  done 
done
