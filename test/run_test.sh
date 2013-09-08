#!/bin/bash

# TODO(dkorolev): HTML format.
# TODO(dkorolev): Get rid of the Makefile and compile from this script with various options.

MAX_ITERATIONS=2000000
TEST_SECONDS=3

cat >/dev/null <<EOF
SAVE_IFS="$IFS"
COMPILERS="g++ clang++"
OPTIONS="-O0 -O3"
CMDLINES=""

for compiler in $COMPILERS ; do
  for options in $OPTIONS ; do
    CMDLINES+=$compiler' '$options':'
  done
done

IFS=':'
for cmdline in $CMDLINES ; do
  echo $cmdline
done
IFS="$SAVE_IFS"
EOF

for function in $(ls functions/ | cut -f1 -d.) ; do 
  echo "Function: "$function
  data=''
  for action in generate evaluate intermediate_evaluate compiled_evaluate; do
    data+=$(./test_binary $function $action $MAX_ITERATIONS $MAX_SECONDS)':'
  done
  echo $function':'$data | awk -F: '{
    name=$1;
    gen_qps = $2;
    gen_eval_qps = $3;
    gen_eval_intermediate_qps = $4;
    gen_eval_compiled_qps = $5;
    compile_time = $6;
    eval_kqps=0.001/(1/gen_eval_qps - 1/gen_qps);
    eval_intermediate_kqps=0.001/(1/gen_eval_intermediate_qps - 1/gen_qps);
    eval_compiled_kqps=0.001/(1/gen_eval_compiled_qps - 1/gen_qps);
    printf ("%20s EvalNative %10.2f kqps, EvalSlow %10.2f kqps, EvalFast %10.2f kqps, %.2fs compilation time.\n", name, eval_kqps, eval_intermediate_kqps, eval_compiled_kqps, compile_time);
  }'
done
