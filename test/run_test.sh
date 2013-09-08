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
  echo "Function: "$function >/dev/stderr
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
    printf ("<tr>");
    printf ("<td align=right>%s</td>", name);
    printf ("<td align=right>%10.2f kqps</td>\n", eval_kqps);
    printf ("<td align=right>%10.2f kqps</td>\n", eval_intermediate_kqps);
    printf ("<td align=right>%10.2f kqps</td>\n", eval_compiled_kqps);
    printf ("<td align=right>%.0f%%</td>\n", 100 * eval_intermediate_kqps / eval_kqps);
    printf ("<td align=right>%.0f%%</td>\n", 100 * eval_compiled_kqps / eval_kqps);
    printf ("<td align=right>%.1fx</td>\n", eval_compiled_kqps / eval_intermediate_kqps);
    printf ("<td align=right>%.2fs</td></tr>\n", compile_time);
    printf ("</tr>");
  }'
done
echo '</table>'
