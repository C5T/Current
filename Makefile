.PHONY: all test individual_tests storage_perftest typesystem_compilation_test check wc clean
.PHONY: individual_tests_1_of_3 individual_tests_1_of_3 individual_tests_3_of_3
.PHONY: individual_tests_1_of_8 individual_tests_2_of_8 individual_tests_3_of_8 individual_tests_4_of_8 individual_tests_5_of_8 individual_tests_6_of_8 individual_tests_7_of_8 individual_tests_8_of_8 individual_tests_all_8
.PHONY: gen_vcxprojs

all: test check

test:
	@./scripts/full-test.sh

individual_tests:
	@./scripts/individual-tests.sh

individual_tests_1_of_3:
	@./scripts/individual-tests.sh 0 3
individual_tests_2_of_3:
	@./scripts/individual-tests.sh 1 3
individual_tests_3_of_3:
	@./scripts/individual-tests.sh 2 3

individual_tests_1_of_8:
	@./scripts/individual-tests.sh 0 8
individual_tests_2_of_8:
	@./scripts/individual-tests.sh 1 8
individual_tests_3_of_8:
	@./scripts/individual-tests.sh 2 8
individual_tests_4_of_8:
	@./scripts/individual-tests.sh 3 8
individual_tests_5_of_8:
	@./scripts/individual-tests.sh 4 8
individual_tests_6_of_8:
	@./scripts/individual-tests.sh 5 8
individual_tests_7_of_8:
	@./scripts/individual-tests.sh 6 8
individual_tests_8_of_8:
	@./scripts/individual-tests.sh 7 8

individual_tests_all_8: individual_tests_1_of_8 individual_tests_2_of_8 individual_tests_3_of_8 individual_tests_4_of_8 individual_tests_5_of_8 individual_tests_6_of_8 individual_tests_7_of_8 individual_tests_8_of_8

storage_perftest:
	(cd examples/Benchmark/generic ; ./run_storage_tests.sh)

typesystem_compilation_test:
	(cd regression_tests/type_system ; ./test.sh 10 50)

typesystem_schema_typescript_tests:
	(cd typesystem/schema ; ./test-c5t-current-schema-ts.sh)

check:
	@./scripts/check-headers.sh

gen_vcxprojs:
	@./scripts/gen_all_vcxprojs.sh

wc:
	echo -n "Total files: " ; (find . -name '*.cc' ; find . -iname '*.h') | grep -v 3rdparty | grep -v "/.current/" | grep -v zzz_full_test | grep -v "/sandbox/" | wc -l
	(find . -name '*.cc' ; find . -iname '*.h') | grep -v 3rdparty | grep -v "/.current/" | grep -v zzz_full_test | grep -v "/sandbox/" | xargs wc -l | sort -gr

clean:
	rm -rf $(shell find $(shell find . -name "zzz_*" -type d) -name "coverage" -type d)
	rm -rf $(shell find $(shell find . -name "zzz_*" -type d) -name "coverage*.info")
	rm -rf $(shell find $(shell find . -name "zzz_*" -type d) -name "everything")
	rm -rf $(shell find $(shell find . -name "zzz_*" -type d) -name "everything.*")
	rm -rf $(shell find . -name ".current")
	rm -rf $(shell find . -name "everything")
	make -f scripts/MakefileImpl clean
