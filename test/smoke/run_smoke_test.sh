#!/bin/bash

BINARY=autogen/eval

SAVE_IFS="$IFS"

# Put together the functions to run perf test against.
IFS=":"
FUNCTIONS_FILES=$(grep INCLUDE_IN_SMOKE_TEST ../f/*.h | cut -f1 -d: | sort -u | paste -sd ':')
FUNCTIONS=''
for i in $FUNCTIONS_FILES ; do
  if [ -n "$FUNCTIONS" ] ; then FUNCTIONS+=':' ; fi
  FUNCTIONS+=$(echo $i | sed 's/\.\.\/f\///g' | sed 's/\.h//g')
done

# Put together the functions that are going to be used by the smoke test.
mkdir -p autogen
rm -f autogen/functions.h
cat ../function.h > autogen/functions.h
for i in $FUNCTIONS_FILES ; do
  cat $i >> autogen/functions.h
done
for i in $FUNCTIONS ; do
  echo 'REGISTER_FUNCTION('$i');' >>autogen/functions.h
done

# Prepare all the command lines.
COMPILERS='g++'  # Just g++, no clang++ in the smoke test.
OPTIONS='-O2'    # No need for fancy optimizations as well.
# TODO(dkorolev): Add machine code generation to the smoke test.
JIT='NASM:CLANG' # Test both compiled implementations.
CMDLINES=''

for compiler in $COMPILERS ; do
  for options in $OPTIONS ; do
    for jit in $JIT ; do
      CMDLINES+=$compiler' --std=c++0x '$options' -DFNCAS_JIT='$jit' ../eval.cc -I $PWD/autogen -o $BINARY -ldl:'
    done
  done
done

# Run the smoke test.
for cmdline in $CMDLINES ; do
  echo $cmdline

  rm -f $BINARY
  eval $cmdline
  if [ ! -x $BINARY ] ; then
    echo 'Compilation error in:'
    echo $cmdline
    IFS="$SAVE_IFS"
    exit 1
  fi

  for function in $FUNCTIONS ; do 
    echo -n $function':'
    data=''
    # Two actions are enough for the smoke test:
    # 1) gen_eval_ieval: Diff native vs. interpreted byte-code computation.
    # 2) gen_eval_ceval: Diff native vs. compiled function compututation.
    # By specifying no extra parameters, the binary runs in the default "smoke test" mode.
    for action in gen_eval_ieval gen_eval_ceval ; do
      result=$(./$BINARY $function $action)
      if [ $? != 0 ] ; then
        echo 'Error '$result' in '$action
        IFS="$SAVE_IFS"
        exit 1
      fi
      echo -n ' OK'
    done
    echo
  done
  rm -f $BINARY
done

echo 'OK'

IFS="$SAVE_IFS"
