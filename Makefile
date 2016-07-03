.PHONY: all test individual_tests check

all: test check

test:
	./scripts/full-test.sh

individual_tests:
	./scripts/individual-tests.sh

check:
	./scripts/check-headers.sh

wc:
	echo -n "Total files: " ; (find . -name '*.cc' ; find . -iname '*.h') | grep -v 3rdparty | grep -v "/.current/" | grep -v zzz_full_test | grep -v "/sandbox/" | wc -l
	(find . -name '*.cc' ; find . -iname '*.h') | grep -v 3rdparty | grep -v "/.current/" | grep -v zzz_full_test | grep -v "/sandbox/" | xargs wc -l | sort -gr

clean:
	rm -rf $(shell find $(shell find . -name "zzz_*" -type d) -name coverage -type d)
	rm -rf $(shell find . -name ".current")
	rm -rf $(shell find . -name "everything")
