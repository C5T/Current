#!/bin/bash

MAX_ITERATIONS=1000000000000 # 10^12
TEST_SECONDS=45

SAVE_IFS="$IFS"

IFS="
"
FUNCTIONS=$(ls functions/ | cut -f1 -d. | paste -sd ':')
IFS=':'

COMPILERS='g++:clang++'
OPTIONS='-O0:-O3'
JIT='NASM:CLANG'
CMDLINES=''

for compiler in $COMPILERS ; do
  for options in $OPTIONS ; do
    for jit in $JIT ; do
      CMDLINES+=$compiler' --std=c++0x '$options' -DFNCAS_JIT='$jit' test.cc -o test_binary -ldl:'
    done
  done
done

echo '<!doctype html>'
echo '<h1>Legend</h1>'
echo 'Regression test validating the results of three evaluations and comparing their runtimes.'
echo '<ul>'
echo '<li>Native: When C (C++, actually) code of the function is compiled by the compiler itself and is being evaluated.</li>'
echo '<li>Intermediate: When the code of the function is parsed into the intermediate format and evaluated by iterpretation.</li>'
echo '<li>Compiled: When the intermediate code is being converted to a source file, compiled and then linked as an .so library.</li>'
echo '</ul>'

for cmdline in $CMDLINES ; do
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

  rm -f test_binary
  eval $cmdline
  if [ ! -x test_binary ] ; then
    echo '</table><hr>'
    echo 'Compilation error.'
    IFS="$SAVE_IFS"
    exit 1
  fi

  for function in $FUNCTIONS ; do 
    echo '  '$function >/dev/stderr
    data=''
    for action in gen gen_eval gen_eval_ieval gen_eval_ceval ; do
      echo -n '    '$action': ' >/dev/stderr
      IFS=''
      result=$(./test_binary $function $action $MAX_ITERATIONS $MAX_SECONDS)
      result_verdict=${result/:*/}
      result_data=${result/$result_verdict:/}
      IFS=':'
      if [ $result_verdict == 'OK' ] ; then
        data+=$result_data':'
      else
        echo '</table><hr>'$result_data
        IFS="$SAVE_IFS"
        exit 1
      fi
      echo $result_data >/dev/stderr
    done
    echo $function' '$data | awk '{
      name=$1;
      gen_qps = $2;
      gen_eval_qps = $3;
      gen_eval_ieval_qps = $4;
      gen_eval_ceval_qps = $5;
      compile_time = $6;
      eval_kqps=0.001/(1/gen_eval_qps - 1/gen_qps);
      ieval_kqps=0.001/(1/gen_eval_ieval_qps - 1/gen_eval_qps);
      ceval_kqps=0.001/(1/gen_eval_ceval_qps - 1/gen_eval_qps);
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
done

echo '<h1>Results</h1>'
echo "The regression test took $SECONDS seconds to run."

IFS="$SAVE_IFS"
