.PHONY: all test check

all: test check

test:
	./scripts/full-test.sh

check:
	./scripts/check-headers.sh

wc:
	echo -n "Total files: " ; (find . -name '*.cc' ; find . -iname '*.h') | grep -v 3rdparty | grep -v "/.current/" | grep -v zzz_full_test | wc -l
	(find . -name '*.cc' ; find . -iname '*.h') | grep -v 3rdparty | grep -v "/.current/" | grep -v zzz_full_test | xargs wc -l | sort -gr

clean:
	rm -rf $(shell find $(shell find . -name "zzz_*" -type d) -name coverage -type d)
	rm -rf $(shell find . -name ".current")
