#!/bin/bash

# TODO(dkorolev): Legend in HTML.
# TODO(dkorolev): Actually compile with different flags.
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

echo '<!doctype html>'
echo '<h1>Legend</h1>'
echo 'TBD'
echo '<h1>Header</h1>'
echo '<table border=1 cellpadding=8>'
echo -n '<tr>'
echo -n '<td align=right>Function</td>'
echo -n '<td align=right>Native, kQPS</td>'
echo -n '<td align=right>Intermediate, kQPS</td>'
echo -n '<td align=right>Compiled, kQPS</td>'
echo -n '<td align=right>Intermediate / Native, %</td>'
echo -n '<td align=right>Compiled / Native, %</td>'
echo -n '<td align=right>Compiled / Intermediate, times</td>'
echo -n '<td align=right>Compilation time, s</td>'
echo '</tr>'
for function in $(ls functions/ | cut -f1 -d.) ; do 
  echo $function >/dev/stderr
  data=''
  for action in gen gen_eval gen_eval_ieval gen_eval_ceval ; do
    echo -n '  '$action': ' >/dev/stderr
    result=$(./test_binary $function $action $MAX_ITERATIONS $MAX_SECONDS)
    echo $result >/dev/stderr
    result_verdict=${result/\:*/}
    result_data=${result/$result_verdict\:/}
    if [ $result_verdict == 'OK' ] ; then
      data+=$result_data':'
    else
      echo '</table><hr>'$result_data
      exit 1
    fi
  done
  echo $function':'$data | awk -F: '{
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
