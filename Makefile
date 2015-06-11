.PHONY: all test check

all: test check

test:
	./scripts/full-test.sh

check:
	./scripts/check-headers.sh
