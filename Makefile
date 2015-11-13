.PHONY: all test check

all: test check

test:
	./scripts/full-test.sh

check:
	./scripts/check-headers.sh

wc:
	echo -n "Total files: " ; (find . -name '*.cc' ; find . -iname '*.h') | grep -v 3rdparty | grep -v "/.current/" | wc -l
	(find . -name '*.cc' ; find . -iname '*.h') | grep -v 3rdparty | grep -v "/.current/" | xargs wc -l | sort -gr

clean:
	rm -rf $(shell find $(shell find . -name "zzz_*" -type d) -name coverage -type d)
	rm -rf $(shell find . -name ".current")
