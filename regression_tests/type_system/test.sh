#!/bin/bash
COUNT_LIST=${*:-10 25 100 500}
TEST_LIST=(
    'current_struct'
    'struct_fields'
    'current_struct_reflection'
    'struct_fields_reflection'
    'typelist'
    'typelist_dynamic'
    'rtti_dynamic_call'
    'variant'
    'json_variant_stringify'
    'json_variant_stringify_and_parse'
    'json_variant_stringify_and_parse_and_test'
    'sherlock_memory'
    'sherlock_file'
    'storage_null_persister'
    'storage_simple_persister'
    'storage_sherlock_persister'
    'storage_with_subscriber'
)

CPLUSPLUS=${CPLUSPLUS:-g++}
CPPFLAGS=${CPPFLAGS:- -std=c++11 -ftemplate-backtrace-limit=0  -ftemplate-depth=10000}
OS=$(uname)
if [[ $OS -ne "Darwin" ]]; then
	LDFLAGS=${LDFLAGS:- -pthread}
fi

mkdir -p .current

# NOTE(dkorolev): The '.current/typelist_impl' test is the same as '.current/typelist', and it's omitted for now.

for c in ${COUNT_LIST[@]}; do
	./gen_test_data.sh $c
	for t in "${TEST_LIST[@]}"; do
		echo -e -n "\033[1m\033[39mBuilding\033[0m "
		echo -e -n "\033[36m"
		echo $t
		echo -e -n "\033[1m\033[91m"
		TIMEFORMAT='Compile: %R seconds'
		time $CPLUSPLUS $CPPFLAGS -c -o .current/$t.o $t.cc >/dev/null
		TIMEFORMAT='Link:    %R seconds'
		time $CPLUSPLUS $LDFLAGS -o .current/$t .current/$t.o >/dev/null
		echo -e -n "\033[1m\033[39mRunning\033[0m: "
		./.current/$t >/dev/null
		if [[ $? -eq 0 ]]; then
			echo -e "\033[32mPASSED\033[39m"
		else
			echo -e "\033[31mFAILED\033[39m"
		fi
		# echo -n "Number of lines in the output of objdump -t:           "
		# objdump -t .current/$t | wc -l
		# echo -n "Average length of the longest 100 lines of objdump -t: "
		# objdump -t .current/$t | awk '{ print length }' | sort -gr | head -n 100 | awk '{ sum += $1 } END { print sum * 0.01 }'
	done
	echo
done
