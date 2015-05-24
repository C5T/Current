#!/bin/bash

BINARY=autogen/eval

# Half a minute per each QPS measurement.
TEST_SECONDS=30

SAVE_IFS="$IFS"

# Put together the functions to run the perf test against.
IFS=":"
FUNCTIONS_FILES=$(grep INCLUDE_IN_PERF_TEST ../f/*.h | cut -f1 -d: | sort -u | paste -sd ':')
FUNCTIONS=''
for i in $FUNCTIONS_FILES ; do
  if [ -n "$FUNCTIONS" ] ; then FUNCTIONS+=':' ; fi
  FUNCTIONS+=$(echo $i | sed 's/\.\.\/f\///g' | sed 's/\.h//g')
done

echo 'Functions: '$FUNCTIONS >/dev/stderr

mkdir -p autogen
cp -f ../function.h autogen/functions.h
for i in $FUNCTIONS_FILES ; do
  cat $i >> autogen/functions.h
done
for i in $FUNCTIONS ; do
  echo 'REGISTER_FUNCTION('$i');' >>autogen/functions.h
done

COMPILERS='g++:clang++'
OPTIONS='-O3'
JIT='NASM:CLANG'
CMDLINES=''

for compiler in $COMPILERS ; do
  for options in $OPTIONS ; do
    for jit in $JIT ; do
      CMDLINES+=$compiler' --std=c++11 '$options' -DFNCAS_JIT='$jit' ../eval.cc -I $PWD/autogen -o $BINARY -ldl:'
    done
  done
done

echo '<!doctype html>'
echo '<h1>Legend</h1>'
echo 'Regression test validating the results of three evaluations and comparing their runtimes.'
echo '<ul>'
echo '<li>Native: When C (C++, actually) code of the function is compiled by the compiler itself and is being evaluated.</li>'
echo '<li>Intermediate: When the code of the function is parsed into the intermediate format and evaluated by interpretation.</li>'
echo '<li>Compiled: When the intermediate code is being converted to a source file, compiled and then linked as an .so library.</li>'
echo '</ul>'

for cmdline in $CMDLINES ; do
  echo $cmdline >/dev/stderr

  echo '<h1>'$cmdline'</h1>'
  echo '<pre>'
  echo '<table border=1 cellpadding=8>'
  echo -n '<tr>'
  echo -n '<td align=right>Function</td>'
  echo -n '<td align=right>Native (N), kQPS</td>'
  echo -n '<td align=right>Intermediate (I), kQPS</td>'
  echo -n '<td align=right>Compiled (C), kQPS</td>'
  echo -n '<td align=right>I/N, %</td>'
  echo -n '<td align=right>C/N, %</td>'
  echo -n '<td align=right>C/I, times</td>'
  echo -n '<td align=right>Compilation time, s</td>'
  echo '</tr>'

  rm -f $BINARY
  eval $cmdline
  if [ ! -x $BINARY ] ; then
    echo '</table><hr>'
    echo 'Compilation error.'
    IFS="$SAVE_IFS"
    exit 1
  fi

  for function in $FUNCTIONS ; do 
    echo '  '$function >/dev/stderr
    data=''
    for action in gen gen_eval_eval gen_eval_ieval gen_eval_ceval ; do
      echo -n '    '$action': ' >/dev/stderr
      result=$(./$BINARY $function $action -$TEST_SECONDS)
      if [ $? != 0 ] ; then
        echo '</table><hr>'$result_data
        IFS="$SAVE_IFS"
        exit 1
      fi
      data+=$result':'
      echo $result >/dev/stderr
    done
    echo $function' '$data | awk '{
      name=$1;
      gen_spq=1/$2;
      gen_eval_eval_spq=1/$3;
      gen_eval_ieval_spq=1/$4;
      gen_eval_ceval_spq=1/$5;
      compile_time=$6;
      gen_eval_spq=(gen_spq+gen_eval_eval_spq)/2;
      eval_kqps=0.001/(gen_eval_spq-gen_spq);
      ieval_kqps=0.001/(gen_eval_ieval_spq-gen_eval_spq);
      ceval_kqps=0.001/(gen_eval_ceval_spq-gen_eval_spq);
      printf ("<tr>\n");
      printf ("<td align=right>%s</td>\n", name);
      printf ("<td align=right>%.2f kqps</td>\n", eval_kqps);
      printf ("<td align=right>%.2f kqps</td>\n", ieval_kqps);
      printf ("<td align=right>%.2f kqps</td>\n", ceval_kqps);
      printf ("<td align=right>%.0f%%</td>\n", 100 * ieval_kqps / eval_kqps);
      printf ("<td align=right>%.0f%%</td>\n", 100 * ceval_kqps / eval_kqps);
      printf ("<td align=right>%.1fx</td>\n", ceval_kqps / ieval_kqps);
      printf ("<td align=right>%.2fs</td></tr>\n", compile_time);
      printf ("</tr>\n");
    }'
  done
  echo '</table>' 
  echo '</pre>' 

  rm -f $BINARY
done

echo '<h1>Results</h1>'
echo "The regression test took $SECONDS seconds to run."

IFS="$SAVE_IFS"
