.PHONY: all test individual_tests storage_perftest typesystem_compilation_test check wc clean

all: test check

test:
	./scripts/full-test.sh

individual_tests:
	./scripts/individual-tests.sh

storage_perftest:
	(cd examples/Benchmark/generic ; ./run_storage_tests.sh)

typesystem_compilation_test:
	(cd regression_tests/type_system ; ./test.sh 10 50)

typesystem_schema_typescript_tests:
	(cd TypeSystem/Schema ; ./test-c5t-current-schema-ts.sh)

check:
	./scripts/check-headers.sh

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
