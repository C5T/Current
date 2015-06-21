.PHONY: all test check

all: test check

test:
	./scripts/full-test.sh

check:
	./scripts/check-headers.sh

clean:
	rm -rf $(shell find $(shell find . -name "zzz_*" -type d) -name coverage -type d)
	rm -rf $(shell find . -name ".current")
