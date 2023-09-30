.PHONY: all test individual_tests storage_perftest typesystem_compilation_test check wc clean
.PHONY: individual_tests_1_of_3 individual_tests_1_of_3 individual_tests_3_of_3
.PHONY: individual_tests_1_of_8 individual_tests_2_of_8 individual_tests_3_of_8 individual_tests_4_of_8 individual_tests_5_of_8 individual_tests_6_of_8 individual_tests_7_of_8 individual_tests_8_of_8 individual_tests_all_8
.PHONY: gen_vcxprojs
.PHONY: cmake_debug cmake_release cmake_debug_dir cmake_release_dir

CMAKE_DEBUG_BUILD_DIR=$(shell echo "$${CMAKE_DEBUG_BUILD_DIR:-.current/.debug}")
CMAKE_RELEASE_BUILD_DIR=$(shell echo "$${CMAKE_RELEASE_BUILD_DIR:-.current/.release}")

OS=$(shell uname)
ifeq ($(OS),Darwin)
  CORES=$(shell sysctl -n hw.physicalcpu)
else
  CORES=$(shell nproc)
endif

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
	rm -rf "${CMAKE_DEBUG_BUILD_DIR}" "${CMAKE_RELEASE_BUILD_DIR}"

cmake_debug: cmake_debug_dir
	@MAKEFLAGS=--no-print-directory cmake --build "${CMAKE_DEBUG_BUILD_DIR}" -j ${CORES}

cmake_debug_dir: ${CMAKE_DEBUG_BUILD_DIR}

${CMAKE_DEBUG_BUILD_DIR}: CMakeLists.txt
	@cmake -B "${CMAKE_DEBUG_BUILD_DIR}" .

cmake_test: cmake_debug
	@(cd "${CMAKE_DEBUG_BUILD_DIR}"; make test)

cmake_release: cmake_release_dir
	@MAKEFLAGS=--no-print-directory cmake --build "${CMAKE_RELEASE_BUILD_DIR}" -j ${CORES}

cmake_release_dir: ${CMAKE_RELEASE_BUILD_DIR}

${CMAKE_RELEASE_BUILD_DIR}: CMakeLists.txt
	@cmake -DCMAKE_BUILD_TYPE=Release -B "${CMAKE_RELEASE_BUILD_DIR}" .

cmake_test_release: cmake_release
	@(cd "${CMAKE_RELEASE_BUILD_DIR}"; make test)
