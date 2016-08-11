#!/bin/bash
COUNT_LIST=${*:-10 25 100 500}
TEST_LIST=(
    '.current/current_struct'
    '.current/struct_fields'
    '.current/typelist'  # This is cheating as `TypeList` now effectively is `TypeListImpl`.
    '.current/typelist_impl'
    '.current/typelist_dynamic'
    '.current/rtti_dynamic_call'
    '.current/variant'
    '.current/storage'
    '.current/storage_with_sherlock'
)

for c in ${COUNT_LIST[@]}; do
	(./gen_test_data.sh $c;	make clean >/dev/null)
	for t in "${TEST_LIST[@]}"; do
		echo -e -n "\033[1m\033[39mCompiling\033[0m: "
		echo -e -n "\033[36m"
		echo $t
		echo -e -n "\033[1m\033[91m"
		TIMEFORMAT='Time elapsed: %R seconds.'
		time {
			make $t >/dev/null
		}
		echo -e -n "\033[1m\033[39mRunning\033[0m: "
		($t >/dev/null)
		if [[ $? -eq 0 ]]; then
			echo -e "\033[32mPASSED\033[39m"
		else
			echo -e "\033[31mFAILED\033[39m"
		fi
	done
	echo
done
